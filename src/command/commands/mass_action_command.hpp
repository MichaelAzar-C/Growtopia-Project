#pragma once
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "../command.hpp"
#include "../command_registry.hpp"
#include "../../packet/packet_helper.hpp"
#include "../../packet/message/input.hpp"
#include "../../world/world.hpp"

namespace command {
// Runs a normal Growtopia chat command (like "/pull <name>") once for every
// other player in the world, with a delay between each one.
// The server still checks permissions: this only works where you could type
// the command yourself (your own world, or where you have access).
class MassActionCommand final : public ICommand {
public:
    MassActionCommand(
        std::string name,
        std::string description,
        std::string game_command
    )
        : name_{ std::move(name) }
        , description_{ std::move(description) }
        , game_command_{ std::move(game_command) }
    { }

    [[nodiscard]] std::string_view name() const override { return name_; }
    [[nodiscard]] std::string description() const override { return description_; }

    Result execute(const Context& ctx) override
    {
        const auto& world{ world::World::instance() };
        const auto local_net_id{ world.get_local_net_id() };

        if (local_net_id < 0) {
            return Result::Failed;
        }

        // Everyone in the world except you
        std::vector<std::string> names;
        for (const auto& [net_id, player] : world.get_players()) {
            if (net_id == local_net_id || !player) {
                continue;
            }

            auto clean{ strip_color_codes(player->name()) };
            if (!clean.empty()) {
                names.push_back(std::move(clean));
            }
        }

        if (names.empty()) {
            return Result::Failed;
        }

        // Stop a previous run of this same command if it's still going
        const std::string tag{ fmt::format("mass_{}", name_) };
        ctx.scheduler->cancel_by_tag(tag);

        const auto client{ &ctx.client };
        for (std::size_t i = 0; i < names.size(); ++i) {
            const std::string chat_text{ fmt::format("/{} {}", game_command_, names[i]) };

            ctx.scheduler->schedule_delayed(
                [client, chat_text] {
                    if (!client->is_connected()) {
                        return;
                    }

                    packet::message::Input input{};
                    input.text = chat_text;
                    std::ignore = packet::PacketHelper::write(input, *client);
                    spdlog::info("Sent: {}", chat_text);
                },
                DELAY_BETWEEN_PLAYERS * static_cast<std::int64_t>(i),
                tag,
                core::TaskPriority::Normal
            );
        }

        return Result::Success;
    }

private:
    // Tiny gap between players so the server's spam protection doesn't
    // drop commands. 10 ms = 30 players in about 0.3 seconds.
    static constexpr std::chrono::milliseconds DELAY_BETWEEN_PLAYERS{ 10 };

    std::string name_;
    std::string description_;
    std::string game_command_;

    // Player names can contain color codes like "`wName``". Remove them.
    [[nodiscard]] static std::string strip_color_codes(const std::string& text)
    {
        std::string result;
        result.reserve(text.size());

        for (std::size_t i = 0; i < text.size(); ++i) {
            if (text[i] == '`') {
                ++i; // skip the backtick and the color character after it
                continue;
            }
            result += text[i];
        }

        return result;
    }
};
}
