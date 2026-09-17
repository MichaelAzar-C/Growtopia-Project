#pragma once
#include <algorithm>
#include <string>
#include <string_view>
#include <tuple>
#include <fmt/format.h>

#include "../command.hpp"
#include "../hotkey_manager.hpp"
#include "../../packet/packet_helper.hpp"
#include "../../packet/generic_packets.hpp"

namespace command {
// Shared with CommandHandler, which reads the popup's answer
inline constexpr std::string_view HOTKEYS_DIALOG{ "proxy_hotkeys" };
inline constexpr std::string_view HOTKEY_COMMAND_FIELD{ "hk_cmd_" };
inline constexpr std::string_view HOTKEY_KEY_FIELD{ "hk_key_" };

class HotkeysCommand final : public ICommand {
public:
    explicit HotkeysCommand(HotkeyManager& manager) : manager_{ manager } { }

    [[nodiscard]] std::string_view name() const override { return "hotkeys"; }
    [[nodiscard]] std::string description() const override { return "Set up to 7 keys or mouse buttons that run commands"; }

    Result execute(const Context& ctx) override
    {
        const auto slots{ manager_.get_slots() };

        std::string dialog{};

        // ---- Header ----
        dialog += "set_default_color|`o\n";
        dialog += fmt::format("add_label_with_icon|big|`wHotkeys``|left|{}|\n", HEADER_ICON);
        dialog += fmt::format("add_smalltext|{}Type a command and a key for each slot. Keys work while Growtopia is focused.``|left|\n", HINT_COLOR);
        dialog += fmt::format("add_smalltext|{}Key examples: F1, P, Space, Ctrl+1, Alt+Shift+Q, Numpad3, Mouse3, Mouse4, Mouse5, F13-F24``|left|\n", HINT_COLOR);
        dialog += "add_quick_exit|\n";
        dialog += "add_spacer|big|\n";

        // ---- 7 slots ----
        for (std::size_t i = 0; i < slots.size(); ++i) {
            dialog += fmt::format("add_label|small|{}Slot {}``|left|\n", NAME_COLOR, i + 1);
            dialog += fmt::format(
                "add_text_input|{}{}|Command|{}|60|\n",
                HOTKEY_COMMAND_FIELD, i + 1, sanitize(slots[i].command)
            );
            dialog += fmt::format(
                "add_text_input|{}{}|Key|{}|30|\n",
                HOTKEY_KEY_FIELD, i + 1, sanitize(slots[i].key)
            );
            dialog += "add_spacer|small|\n";
        }

        // ---- Footer ----
        dialog += fmt::format("end_dialog|{}|Cancel|Save|\n", HOTKEYS_DIALOG);

        packet::GenericVariantPacket pkt{};
        pkt.game_packet.net_id = -1;
        pkt.variant = packet::PacketVariant{ "OnDialogRequest", dialog };
        std::ignore = packet::PacketHelper::write(pkt, ctx.server);

        return Result::Success;
    }

private:
    static constexpr int HEADER_ICON{ 32 };
    static constexpr std::string_view NAME_COLOR{ "`$" }; // yellow
    static constexpr std::string_view HINT_COLOR{ "`7" }; // faded gray

    HotkeyManager& manager_;

    [[nodiscard]] static std::string sanitize(std::string text)
    {
        std::replace(text.begin(), text.end(), '|', '/');
        std::replace(text.begin(), text.end(), '\n', ' ');
        std::replace(text.begin(), text.end(), '\r', ' ');
        return text;
    }
};
}
