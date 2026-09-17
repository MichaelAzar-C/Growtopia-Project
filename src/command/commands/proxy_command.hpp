#pragma once
#include <string>
#include <string_view>
#include <tuple>
#include <algorithm>
#include <fmt/format.h>

#include "../command.hpp"
#include "../command_registry.hpp"
#include "../../packet/packet_helper.hpp"
#include "../../packet/generic_packets.hpp"

namespace command {
// Shared with CommandHandler, which reads the popup's answer
inline constexpr std::string_view QUICK_PROXY_DIALOG{ "quick_proxy" };
inline constexpr std::string_view QUICK_CHECKBOX_PREFIX{ "qc_" };

class ProxyCommand final : public ICommand {
public:
    [[nodiscard]] std::string_view name() const override { return "proxy"; }
    [[nodiscard]] std::string description() const override { return "Open the Quick Proxy Commands popup"; }

    Result execute(const Context& ctx) override
    {
        const auto quick_commands{ ctx.registry.get_quick_commands() };

        std::string dialog{};

        // ---- Header ----
        dialog += "set_default_color|`o\n";
        dialog += fmt::format("add_label_with_icon|big|`wQuick Proxy Commands``|left|{}|\n", HEADER_ICON);
        dialog += "add_smalltext|`8Tick a box to enable a command, untick to disable it.``|left|\n";
        dialog += "add_quick_exit|\n";
        dialog += "add_spacer|big|\n";

        // ---- Empty state ----
        if (quick_commands.empty()) {
            dialog += "add_textbox|`oNo quick commands added yet.``|left|\n";
            dialog += fmt::format("end_dialog|{}|Close||\n", QUICK_PROXY_DIALOG);
            send_dialog(ctx, dialog);
            return Result::Success;
        }

        // ---- One tick box per quick command ----
        for (const auto& info : quick_commands) {
            dialog += fmt::format(
                "add_checkbox|{}{}|`9{}{}``|{}|\n",
                QUICK_CHECKBOX_PREFIX,
                info.name,
                ctx.registry.prefix(),
                sanitize(info.name),
                info.enabled ? 1 : 0
            );
            dialog += fmt::format(
                "add_smalltext|`o{}``|left|\n",
                sanitize(info.description.empty() ? "No description" : info.description)
            );
            dialog += "add_spacer|small|\n";
        }

        // ---- Footer: Cancel closes, Apply saves the tick boxes ----
        dialog += fmt::format("end_dialog|{}|Cancel|Apply|\n", QUICK_PROXY_DIALOG);

        send_dialog(ctx, dialog);
        return Result::Success;
    }

private:
    static constexpr int HEADER_ICON{ 32 };

    static void send_dialog(const Context& ctx, const std::string& dialog)
    {
        packet::GenericVariantPacket pkt{};
        pkt.game_packet.net_id = -1;
        pkt.variant = packet::PacketVariant{ "OnDialogRequest", dialog };
        std::ignore = packet::PacketHelper::write(pkt, ctx.server);
    }

    [[nodiscard]] static std::string sanitize(std::string text)
    {
        std::replace(text.begin(), text.end(), '|', '/');
        std::replace(text.begin(), text.end(), '\n', ' ');
        std::replace(text.begin(), text.end(), '\r', ' ');
        return text;
    }
};
}
