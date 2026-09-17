#pragma once
#include <tuple>

#include "../command.hpp"
#include "../../packet/packet_helper.hpp"
#include "../../packet/message/chat.hpp"

namespace command {
// Example quick command, so the /proxy popup has something to show.
// Replace or remove it once you add your own quick commands.
class PingCommand final : public ICommand {
public:
    [[nodiscard]] std::string_view name() const override { return "ping"; }
    [[nodiscard]] std::string description() const override { return "Example quick command: replies with Pong"; }

    Result execute(const Context& ctx) override
    {
        packet::message::Log log{};
        log.msg = "`2Pong!``";
        std::ignore = packet::PacketHelper::write(log, ctx.server);
        return Result::Success;
    }
};
}
