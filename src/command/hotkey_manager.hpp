#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace command {
// One hotkey slot as the user typed it in the /hotkeys popup
struct HotkeySlot {
    std::string command; // e.g. "/pullall" or "/warp START"
    std::string key;     // e.g. "F1", "P", "Ctrl+1", "Mouse4"
};

// A key name turned into something we can compare against real key presses
struct KeyBinding {
    std::uint32_t virtual_key{ 0 }; // Windows virtual-key code
    bool ctrl{ false };
    bool alt{ false };
    bool shift{ false };
};

// Listens for the saved keys and mouse buttons while Growtopia is the
// active window, and calls on_trigger with the slot's command.
// Keys are passed through to the game as well (they are not blocked).
// Slots are saved to hotkeys.txt next to Proxy.exe.
class HotkeyManager {
public:
    static constexpr std::size_t SLOT_COUNT{ 7 };
    using Slots = std::array<HotkeySlot, SLOT_COUNT>;
    using TriggerCallback = std::function<void(const std::string& command)>;

    explicit HotkeyManager(TriggerCallback on_trigger);
    ~HotkeyManager();

    HotkeyManager(const HotkeyManager&) = delete;
    HotkeyManager& operator=(const HotkeyManager&) = delete;

    [[nodiscard]] Slots get_slots() const;

    // Replaces all slots, drops keys that can't be understood, and saves
    void set_slots(const Slots& slots);

    // "Ctrl+Shift+P" -> KeyBinding, or nothing if the name isn't known
    [[nodiscard]] static std::optional<KeyBinding> parse_key(const std::string& text);

    // Called by the Windows listener (internal)
    void on_key_down(std::uint32_t virtual_key, bool ctrl, bool alt, bool shift);

private:
    void load();
    void save() const;
    void start_listener();
    void stop_listener();

    TriggerCallback on_trigger_;

    mutable std::mutex mutex_;
    Slots slots_{};
    std::array<std::optional<KeyBinding>, SLOT_COUNT> bindings_{};

    std::thread listener_thread_;
    std::atomic<std::uint32_t> listener_thread_id_{ 0 };
};
}
