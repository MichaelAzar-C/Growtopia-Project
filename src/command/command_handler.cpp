#include "command_handler.hpp"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <tuple>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "../packet/packet_helper.hpp"
#include "../packet/message/input.hpp"
#include "commands/debug_command.hpp"
#include "commands/exit_command.hpp"
#include "commands/help_command.hpp"
#include "commands/nick_command.hpp"
#include "commands/proxy_command.hpp"
#include "commands/skin_command.hpp"
#include "commands/warp_command.hpp"
#include "commands/ping_command.hpp"
#include "commands/mass_action_command.hpp"
#include "commands/hotkeys_command.hpp"
#include "../packet/chat_packet.hpp"
#include "../packet/generic_packets.hpp"
#include "../packet/game/world.hpp"
#include "../packet/message/chat.hpp"
#include "../utils/text_parse.hpp"

namespace command {
namespace {
    constexpr auto input_event_type = event::packet_event_type(packet::PacketId::Input);
    constexpr auto spawn_event_type = event::packet_event_type(packet::PacketId::OnSpawn);
}

CommandHandler::CommandHandler(
    core::Config& config,
    event::Dispatcher& dispatcher,
    std::shared_ptr<core::Scheduler> scheduler,
    network::Server& server,
    network::Client& client
)
    : config_{ config }
    , dispatcher_{ dispatcher }
    , scheduler_{ std::move(scheduler) }
    , server_{ server }
    , client_{ client }
{
    registry_.set_prefix(config_.get_command_config().prefix);
    // Hotkeys: load saved keys and start listening. Commands run on the
    // scheduler so the keyboard listener is never slowed down.
    hotkeys_ = std::make_unique<HotkeyManager>([this](const std::string& command) {
        scheduler_->schedule_delayed(
            [this, command] { run_hotkey_command(command); },
            std::chrono::milliseconds{ 0 },
            "hotkey",
            core::TaskPriority::Normal
        );
    });

    register_default_commands();

    listener_handle_ = dispatcher_.prepend_listener(
        input_event_type,
        [this](const event::Event& e) { on_text_packet(e); }
    );

    // Catch the answer from the Quick Proxy Commands popup before it
    // reaches the Growtopia server.
    dialog_listener_handle_ = dispatcher_.prepend_listener(
        event::Type::ServerBoundPacket,
        [this](const event::Event& e) { on_raw_server_bound(e); }
    );

    // Extended zoom (always on): adjust our own spawn packet. Appended (normal priority)
    // so the world handler still records the player first.
    spawn_listener_handle_ = dispatcher_.append_listener(
        spawn_event_type,
        [this](const event::Event& e) { on_spawn(e); }
    );

    spdlog::info("Command handler initialized with prefix '{}'", registry_.prefix());
}

CommandHandler::~CommandHandler()
{
    // Stop the keyboard listener first so no new hotkey tasks are queued
    hotkeys_.reset();
    scheduler_->cancel_by_tag("hotkey");

    dispatcher_.remove_listener(input_event_type, listener_handle_);
    dispatcher_.remove_listener(event::Type::ServerBoundPacket, dialog_listener_handle_);
    dispatcher_.remove_listener(spawn_event_type, spawn_listener_handle_);
}

void CommandHandler::register_default_commands()
{
    registry_.add(std::make_unique<ProxyCommand>());
    registry_.add(std::make_unique<WarpCommand>());
    registry_.add(std::make_unique<ExitCommand>());
    registry_.add(std::make_unique<NickCommand>());
    registry_.add(std::make_unique<SkinCommand>());
    registry_.add(std::make_unique<HelpCommand>());
    registry_.add(std::make_unique<DebugCommand>());

    // Built-in mass actions (always enabled, listed in /phelp)
    registry_.add(std::make_unique<MassActionCommand>(
        "pullall", "Pull every player in the world at once", "pull"
    ));
    registry_.add(std::make_unique<MassActionCommand>(
        "banall", "Ban every player in the world at once", "ban"
    ));
    registry_.add(std::make_unique<HotkeysCommand>(*hotkeys_));

    // Quick commands (shown in /proxy with a tick box, hidden from /phelp)
    registry_.add_quick(std::make_unique<PingCommand>(), true);


}

void CommandHandler::on_text_packet(const event::Event& e)
{
    const auto* evt = dynamic_cast<const event::TypedPacketEvent<packet::PacketId::Input>*>(&e);
    if (!evt) {
        return;
    }

    const auto input_pkt{ evt->get<packet::message::Input>() };
    if (!input_pkt) {
        return;
    }

    const std::string& text = input_pkt->text;
    if (registry_.execute(text, server_, client_, dispatcher_, scheduler_)) {
        spdlog::info("Command handler executed successfully");
        evt->cancel();
    }
}

void CommandHandler::on_raw_server_bound(const event::Event& e)
{
    const auto* raw = dynamic_cast<const event::RawPacketEvent*>(&e);
    if (!raw || raw->data.size() <= sizeof(std::uint32_t)) {
        return;
    }

    // Only text messages can be a dialog answer
    std::uint32_t message_type{};
    std::memcpy(&message_type, raw->data.data(), sizeof(message_type));
    if (
        message_type != packet::NET_MESSAGE_GENERIC_TEXT &&
        message_type != packet::NET_MESSAGE_GAME_MESSAGE
    ) {
        return;
    }

    std::string text{
        reinterpret_cast<const char*>(raw->data.data()) + sizeof(message_type),
        raw->data.size() - sizeof(message_type)
    };
    while (!text.empty() && (text.back() == '\0' || text.back() == '\n' || text.back() == '\r')) {
        text.pop_back();
    }

    if (text.find("dialog_return") == std::string::npos) {
        return;
    }

    const utils::TextParse parse{ text };
    if (parse.get("action") != "dialog_return") {
        return;
    }

    // ---- /hotkeys popup ----
    if (parse.get("dialog_name") == HOTKEYS_DIALOG) {
        e.cancel(); // our popup: the server must never see this

        HotkeyManager::Slots slots{};
        for (std::size_t i = 0; i < slots.size(); ++i) {
            slots[i].command = parse.get(fmt::format("{}{}", HOTKEY_COMMAND_FIELD, i + 1));
            slots[i].key = parse.get(fmt::format("{}{}", HOTKEY_KEY_FIELD, i + 1));
        }
        hotkeys_->set_slots(slots);
        spdlog::info("Hotkeys saved");
        return;
    }

    if (parse.get("dialog_name") != QUICK_PROXY_DIALOG) {
        return; // A real Growtopia popup: let it through
    }

    // This answer belongs to our popup, so the server must never see it
    e.cancel();

    std::size_t changed{ 0 };
    for (const auto& info : registry_.get_quick_commands()) {
        const std::string value{
            parse.get(fmt::format("{}{}", QUICK_CHECKBOX_PREFIX, info.name))
        };
        if (value.empty()) {
            continue;
        }

        const bool enabled{ value.front() == '1' };
        if (enabled != info.enabled) {
            registry_.set_enabled(info.name, enabled);
            ++changed;
        }

        spdlog::info("Quick command '{}' is now {}", info.name, enabled ? "enabled" : "disabled");
    }

    packet::message::Log log{};
    log.msg = fmt::format("`2Quick commands saved ``({} changed)", changed);
    std::ignore = packet::PacketHelper::write(log, server_);
}

void CommandHandler::on_spawn(const event::Event& e)
{
    // Extended zoom is always on: Growtopia gives moderators a wider zoom
    // range, and reads that from the "mstate" line of YOUR spawn packet.
    // We set it to 1 for the local player only. Nothing goes to the server.
    const auto* evt = dynamic_cast<const event::TypedPacketEvent<packet::PacketId::OnSpawn>*>(&e);
    if (!evt || evt->direction != event::Direction::ClientBound) {
        return;
    }

    const auto pkt{ evt->get<packet::game::OnSpawn>() };
    if (!pkt || pkt->type != "local" || pkt->variant.size() < 2) {
        return; // Only change OUR player, never other players
    }

    // Edit only the "mstate" line of the original text, keeping every
    // other line exactly as the server sent it.
    std::string text{ pkt->variant.get<std::string>(1) };
    const auto line_start{ text.find("mstate|") };
    if (line_start != std::string::npos && (line_start == 0 || text[line_start - 1] == '\n')) {
        const auto value_start{ line_start + std::string_view{ "mstate|" }.size() };
        const auto line_end{ text.find('\n', value_start) };
        text.replace(
            value_start,
            (line_end == std::string::npos ? text.size() : line_end) - value_start,
            "1"
        );
    }
    else {
        if (!text.empty() && text.back() != '\n') {
            text += '\n';
        }
        text += "mstate|1\n";
    }

    packet::GenericVariantPacket modified{};
    modified.game_packet = pkt->game_packet;
    modified.variant = pkt->variant;
    modified.variant.set(1, text);

    // Send our edited copy to the game instead of the original
    e.cancel();
    std::ignore = packet::PacketHelper::write(modified, server_);
    spdlog::info("Extended zoom applied to local player");
}

void CommandHandler::run_hotkey_command(const std::string& command)
{
    if (command.empty() || !client_.is_connected()) {
        return;
    }

    // Proxy commands (like /pullall) run inside the proxy
    if (registry_.execute(command, server_, client_, dispatcher_, scheduler_)) {
        return;
    }

    // Anything else (like /warp START or normal chat) goes to the server
    std::ignore = client_.write(packet::build_chat_packet(command));
}
}
