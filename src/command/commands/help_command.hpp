#pragma once
#include <algorithm>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
#include <fmt/format.h>

#include "../command.hpp"
#include "../command_registry.hpp"
#include "../../packet/packet_helper.hpp"
#include "../../packet/generic_packets.hpp"
#include "../../packet/message/chat.hpp"

namespace command {
class HelpCommand final : public ICommand {
public:
    [[nodiscard]] std::string_view name() const override { return "phelp"; }
    [[nodiscard]] std::string description() const override { return "Open a popup listing all proxy commands"; }

    Result execute(const Context& ctx) override
    {
        // "/phelp <command>" -> show info for one command in chat
        if (!ctx.args.empty()) {
            return show_single_command(ctx, ctx.args[0]);
        }

        // "/phelp" -> open the popup with every command
        auto commands{ ctx.registry.get_all_commands() };
        std::sort(commands.begin(), commands.end(), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });

        // Don't list /phelp itself, quick commands (those live in /proxy),
        // or hidden built-in commands
        std::erase_if(commands, [this, &ctx](const auto& entry) {
            return entry.first == name()
                || ctx.registry.is_quick(entry.first)
                || ctx.registry.is_hidden(entry.first);
        });

        std::string dialog{};

        // ---- Header ----
        dialog += "set_default_color|`o\n";
        dialog += fmt::format("add_label_with_icon|big|`wProxy Commands``|left|{}|\n", HEADER_ICON);
        dialog += fmt::format("add_smalltext|`8GTProxy `o- `w{} ``commands available|left|\n", commands.size());
        dialog += "add_quick_exit|\n";
        dialog += "add_spacer|big|\n";

        // ---- Command list ----
        if (commands.empty()) {
            dialog += "add_textbox|`oNo commands added yet.``|left|\n";
        }

        for (const auto& [cmd_name, desc] : commands) {
            // Command name with a small icon
            dialog += fmt::format(
                "add_label_with_icon|small|`9{}{}``|left|{}|\n",
                ctx.registry.prefix(),
                sanitize(cmd_name),
                COMMAND_ICON
            );

            // Description in small text under the name
            dialog += fmt::format(
                "add_smalltext|`o{}``|left|\n",
                sanitize(desc.empty() ? "No description" : desc)
            );

            dialog += "add_spacer|small|\n";
        }

        // ---- Footer ----
        dialog += "add_spacer|small|\n";
        dialog += "end_dialog|proxy_phelp|Close||\n";

        packet::GenericVariantPacket pkt{};
        pkt.game_packet.net_id = -1;
        pkt.variant = packet::PacketVariant{ "OnDialogRequest", dialog };

        std::ignore = packet::PacketHelper::write(pkt, ctx.server);
        return Result::Success;
    }

private:
    // Item IDs used as icons in the popup. Change these to any item ID you like.
    static constexpr int HEADER_ICON{ 32 };
    static constexpr int COMMAND_ICON{ 32 };

    // Dialog lines are separated by '|' and newlines, so a description
    // containing those characters would break the popup layout.
    [[nodiscard]] static std::string sanitize(std::string text)
    {
        std::replace(text.begin(), text.end(), '|', '/');
        std::replace(text.begin(), text.end(), '\n', ' ');
        std::replace(text.begin(), text.end(), '\r', ' ');
        return text;
    }

    [[nodiscard]] static Result show_single_command(const Context& ctx, const std::string& cmd_name)
    {
        if (auto cmd{ ctx.registry.get(cmd_name) }) {
            packet::message::Log log_info{};
            log_info.msg = fmt::format("``{}{}: {}", ctx.registry.prefix(), cmd_name, cmd->description());
            std::ignore = packet::PacketHelper::write(log_info, ctx.server);
            return Result::Success;
        }

        packet::message::Log log_error{};
        log_error.msg = fmt::format("`4Error: ``Command '{}' not found", cmd_name);
        std::ignore = packet::PacketHelper::write(log_error, ctx.server);
        return Result::Failed;
    }
};
}
