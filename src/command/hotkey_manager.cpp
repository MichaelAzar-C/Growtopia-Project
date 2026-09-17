#include "hotkey_manager.hpp"

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <fstream>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace command {
namespace {
constexpr const char* HOTKEYS_FILE{ "hotkeys.txt" };

// Virtual-key codes, written out so this file also compiles on non-Windows
constexpr std::uint32_t VK_LBUTTON_{ 0x01 };
constexpr std::uint32_t VK_RBUTTON_{ 0x02 };
constexpr std::uint32_t VK_MBUTTON_{ 0x04 };
constexpr std::uint32_t VK_XBUTTON1_{ 0x05 };
constexpr std::uint32_t VK_XBUTTON2_{ 0x06 };
constexpr std::uint32_t VK_SHIFT_{ 0x10 };
constexpr std::uint32_t VK_CONTROL_{ 0x11 };
constexpr std::uint32_t VK_MENU_{ 0x12 };
constexpr std::uint32_t VK_LSHIFT_{ 0xA0 };
constexpr std::uint32_t VK_RSHIFT_{ 0xA1 };
constexpr std::uint32_t VK_LCONTROL_{ 0xA2 };
constexpr std::uint32_t VK_RCONTROL_{ 0xA3 };
constexpr std::uint32_t VK_LMENU_{ 0xA4 };
constexpr std::uint32_t VK_RMENU_{ 0xA5 };

std::string to_lower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

std::string trim(const std::string& text)
{
    const auto begin{ text.find_first_not_of(" \t\r\n") };
    if (begin == std::string::npos) {
        return {};
    }
    const auto end{ text.find_last_not_of(" \t\r\n") };
    return text.substr(begin, end - begin + 1);
}

const std::unordered_map<std::string, std::uint32_t>& key_names()
{
    static const std::unordered_map<std::string, std::uint32_t> names{ [] {
        std::unordered_map<std::string, std::uint32_t> map{
            // Mouse
            { "mouse1", VK_LBUTTON_ }, { "leftclick", VK_LBUTTON_ },
            { "mouse2", VK_RBUTTON_ }, { "rightclick", VK_RBUTTON_ },
            { "mouse3", VK_MBUTTON_ }, { "middleclick", VK_MBUTTON_ },
            { "mouse4", VK_XBUTTON1_ },
            { "mouse5", VK_XBUTTON2_ },

            // Modifiers as keys on their own
            { "shift", VK_SHIFT_ }, { "lshift", VK_LSHIFT_ }, { "rshift", VK_RSHIFT_ },
            { "ctrl", VK_CONTROL_ }, { "control", VK_CONTROL_ },
            { "lctrl", VK_LCONTROL_ }, { "rctrl", VK_RCONTROL_ },
            { "alt", VK_MENU_ }, { "lalt", VK_LMENU_ }, { "ralt", VK_RMENU_ },

            // Common keys
            { "backspace", 0x08 }, { "tab", 0x09 }, { "enter", 0x0D }, { "return", 0x0D },
            { "pause", 0x13 }, { "capslock", 0x14 }, { "esc", 0x1B }, { "escape", 0x1B },
            { "space", 0x20 }, { "pageup", 0x21 }, { "pagedown", 0x22 },
            { "end", 0x23 }, { "home", 0x24 },
            { "left", 0x25 }, { "up", 0x26 }, { "right", 0x27 }, { "down", 0x28 },
            { "printscreen", 0x2C }, { "insert", 0x2D }, { "delete", 0x2E }, { "del", 0x2E },
            { "lwin", 0x5B }, { "rwin", 0x5C }, { "menu", 0x5D },
            { "numlock", 0x90 }, { "scrolllock", 0x91 },

            // Numpad extras
            { "numpad*", 0x6A }, { "numpadmultiply", 0x6A },
            { "numpad+", 0x6B }, { "numpadadd", 0x6B },
            { "numpad-", 0x6D }, { "numpadsubtract", 0x6D },
            { "numpad.", 0x6E }, { "numpaddecimal", 0x6E },
            { "numpad/", 0x6F }, { "numpaddivide", 0x6F },

            // Symbols (US layout names and characters)
            { ";", 0xBA }, { "semicolon", 0xBA },
            { "=", 0xBB }, { "equals", 0xBB }, { "plus", 0xBB },
            { ",", 0xBC }, { "comma", 0xBC },
            { "-", 0xBD }, { "minus", 0xBD },
            { ".", 0xBE }, { "period", 0xBE },
            { "/", 0xBF }, { "slash", 0xBF },
            { "`", 0xC0 }, { "backtick", 0xC0 }, { "tilde", 0xC0 },
            { "[", 0xDB }, { "leftbracket", 0xDB },
            { "\\", 0xDC }, { "backslash", 0xDC },
            { "]", 0xDD }, { "rightbracket", 0xDD },
            { "'", 0xDE }, { "quote", 0xDE },
        };

        for (char c = 'a'; c <= 'z'; ++c) {
            map[std::string(1, c)] = static_cast<std::uint32_t>(std::toupper(c));
        }
        for (char c = '0'; c <= '9'; ++c) {
            map[std::string(1, c)] = static_cast<std::uint32_t>(c);
            map["numpad" + std::string(1, c)] = 0x60 + static_cast<std::uint32_t>(c - '0');
        }
        for (int i = 1; i <= 24; ++i) {
            map["f" + std::to_string(i)] = 0x70 + static_cast<std::uint32_t>(i - 1);
        }

        return map;
    }() };

    return names;
}

bool is_modifier_key(const std::uint32_t vk)
{
    return vk == VK_SHIFT_ || vk == VK_CONTROL_ || vk == VK_MENU_
        || (vk >= VK_LSHIFT_ && vk <= VK_RMENU_);
}

// Left/right modifier keys should also match a plain "Ctrl"/"Shift"/"Alt"
std::uint32_t generic_modifier(const std::uint32_t vk)
{
    switch (vk) {
    case VK_LSHIFT_: case VK_RSHIFT_: return VK_SHIFT_;
    case VK_LCONTROL_: case VK_RCONTROL_: return VK_CONTROL_;
    case VK_LMENU_: case VK_RMENU_: return VK_MENU_;
    default: return vk;
    }
}
}

HotkeyManager::HotkeyManager(TriggerCallback on_trigger)
    : on_trigger_{ std::move(on_trigger) }
{
    load();
    start_listener();
}

HotkeyManager::~HotkeyManager()
{
    stop_listener();
}

HotkeyManager::Slots HotkeyManager::get_slots() const
{
    std::scoped_lock lock{ mutex_ };
    return slots_;
}

void HotkeyManager::set_slots(const Slots& slots)
{
    {
        std::scoped_lock lock{ mutex_ };
        for (std::size_t i = 0; i < SLOT_COUNT; ++i) {
            slots_[i].command = trim(slots[i].command);
            slots_[i].key = trim(slots[i].key);

            bindings_[i] = parse_key(slots_[i].key);
            if (!bindings_[i]) {
                slots_[i].key.clear(); // unknown key name: empty the key box
            }
        }
    }

    save();
}

std::optional<KeyBinding> HotkeyManager::parse_key(const std::string& text)
{
    const std::string cleaned{ trim(text) };
    if (cleaned.empty()) {
        return std::nullopt;
    }

    // Split on '+', but allow "+" itself as the last part (e.g. "Ctrl++")
    std::vector<std::string> parts;
    std::string current;
    for (std::size_t i = 0; i < cleaned.size(); ++i) {
        const char c{ cleaned[i] };
        if (c == '+' && !current.empty()) {
            parts.push_back(trim(current));
            current.clear();
        }
        else {
            current += c;
        }
    }
    if (!current.empty()) {
        parts.push_back(trim(current));
    }
    if (parts.empty()) {
        return std::nullopt;
    }

    KeyBinding binding{};

    // Everything except the last part must be a modifier
    for (std::size_t i = 0; i + 1 < parts.size(); ++i) {
        const auto part{ to_lower(parts[i]) };
        if (part == "ctrl" || part == "control") {
            binding.ctrl = true;
        }
        else if (part == "alt") {
            binding.alt = true;
        }
        else if (part == "shift") {
            binding.shift = true;
        }
        else {
            return std::nullopt;
        }
    }

    auto last{ to_lower(parts.back()) };
    if (last == "+") {
        last = "plus";
    }

    const auto& names{ key_names() };
    const auto it{ names.find(last) };
    if (it == names.end()) {
        return std::nullopt;
    }

    binding.virtual_key = it->second;
    return binding;
}

void HotkeyManager::on_key_down(
    const std::uint32_t virtual_key,
    const bool ctrl,
    const bool alt,
    const bool shift)
{
    std::vector<std::string> to_run;

    {
        std::scoped_lock lock{ mutex_ };
        for (std::size_t i = 0; i < SLOT_COUNT; ++i) {
            const auto& binding{ bindings_[i] };
            if (!binding || slots_[i].command.empty()) {
                continue;
            }

            const bool key_matches{
                binding->virtual_key == virtual_key
                || binding->virtual_key == generic_modifier(virtual_key)
            };
            if (!key_matches) {
                continue;
            }

            // Modifier keys bound on their own ignore the modifier state
            if (!is_modifier_key(binding->virtual_key)) {
                if (binding->ctrl != ctrl || binding->alt != alt || binding->shift != shift) {
                    continue;
                }
            }

            to_run.push_back(slots_[i].command);
        }
    }

    for (const auto& command : to_run) {
        spdlog::info("Hotkey pressed: {}", command);
        if (on_trigger_) {
            on_trigger_(command);
        }
    }
}

void HotkeyManager::load()
{
    std::ifstream file{ HOTKEYS_FILE };
    if (!file) {
        return; // no saved hotkeys yet
    }

    std::scoped_lock lock{ mutex_ };
    std::string line;
    std::size_t index{ 0 };

    // One line per slot: <key><TAB><command>
    while (index < SLOT_COUNT && std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        const auto tab{ line.find('\t') };
        if (tab == std::string::npos) {
            slots_[index] = {};
        }
        else {
            slots_[index].key = trim(line.substr(0, tab));
            slots_[index].command = trim(line.substr(tab + 1));
        }

        bindings_[index] = parse_key(slots_[index].key);
        ++index;
    }

    spdlog::info("Loaded hotkeys from {}", HOTKEYS_FILE);
}

void HotkeyManager::save() const
{
    std::ofstream file{ HOTKEYS_FILE, std::ios::trunc };
    if (!file) {
        spdlog::warn("Could not save {}", HOTKEYS_FILE);
        return;
    }

    std::scoped_lock lock{ mutex_ };
    for (const auto& slot : slots_) {
        file << slot.key << '\t' << slot.command << '\n';
    }
}

#ifdef _WIN32
namespace {
HotkeyManager* g_manager{ nullptr };

// True only when the focused window belongs to Growtopia.exe
bool is_growtopia_focused()
{
    const HWND window{ GetForegroundWindow() };
    if (!window) {
        return false;
    }

    DWORD process_id{ 0 };
    GetWindowThreadProcessId(window, &process_id);
    if (process_id == 0) {
        return false;
    }

    const HANDLE process{ OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id) };
    if (!process) {
        return false;
    }

    wchar_t path[MAX_PATH]{};
    DWORD size{ MAX_PATH };
    const bool ok{ QueryFullProcessImageNameW(process, 0, path, &size) != 0 };
    CloseHandle(process);
    if (!ok) {
        return false;
    }

    std::wstring_view full{ path, size };
    const auto slash{ full.find_last_of(L"\\/") };
    std::wstring name{ slash == std::wstring_view::npos ? full : full.substr(slash + 1) };
    std::transform(name.begin(), name.end(), name.begin(), [](wchar_t c) {
        return static_cast<wchar_t>(std::towlower(c));
    });

    return name == L"growtopia.exe";
}

bool is_down(const int vk)
{
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

void report_key(const std::uint32_t vk)
{
    if (!g_manager || !is_growtopia_focused()) {
        return;
    }

    g_manager->on_key_down(
        vk,
        is_down(VK_CONTROL),
        is_down(VK_MENU),
        is_down(VK_SHIFT)
    );
}

// Remembers which keys are held, so holding a key only fires once
std::array<bool, 256> g_held{};

LRESULT CALLBACK keyboard_hook(const int code, const WPARAM wparam, const LPARAM lparam)
{
    if (code == HC_ACTION) {
        const auto* info{ reinterpret_cast<const KBDLLHOOKSTRUCT*>(lparam) };
        const auto vk{ static_cast<std::uint32_t>(info->vkCode & 0xFF) };

        if (wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN) {
            if (!g_held[vk]) {
                g_held[vk] = true;
                report_key(vk);
            }
        }
        else if (wparam == WM_KEYUP || wparam == WM_SYSKEYUP) {
            g_held[vk] = false;
        }
    }

    // Always pass the key on to the game and other programs
    return CallNextHookEx(nullptr, code, wparam, lparam);
}

LRESULT CALLBACK mouse_hook(const int code, const WPARAM wparam, const LPARAM lparam)
{
    if (code == HC_ACTION) {
        const auto* info{ reinterpret_cast<const MSLLHOOKSTRUCT*>(lparam) };

        switch (wparam) {
        case WM_LBUTTONDOWN: report_key(VK_LBUTTON_); break;
        case WM_RBUTTONDOWN: report_key(VK_RBUTTON_); break;
        case WM_MBUTTONDOWN: report_key(VK_MBUTTON_); break;
        case WM_XBUTTONDOWN:
            report_key(HIWORD(info->mouseData) == XBUTTON1 ? VK_XBUTTON1_ : VK_XBUTTON2_);
            break;
        default:
            break;
        }
    }

    // Always pass the click on to the game and other programs
    return CallNextHookEx(nullptr, code, wparam, lparam);
}
}

void HotkeyManager::start_listener()
{
    g_manager = this;

    listener_thread_ = std::thread{ [this] {
        listener_thread_id_ = GetCurrentThreadId();

        const HINSTANCE module{ GetModuleHandleW(nullptr) };
        const HHOOK keyboard{ SetWindowsHookExW(WH_KEYBOARD_LL, keyboard_hook, module, 0) };
        const HHOOK mouse{ SetWindowsHookExW(WH_MOUSE_LL, mouse_hook, module, 0) };

        if (!keyboard || !mouse) {
            spdlog::warn("Hotkeys: could not start the keyboard/mouse listener");
        }
        else {
            spdlog::info("Hotkeys listener started");
        }

        // Low-level hooks need a message loop on the thread that set them
        MSG msg{};
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        if (keyboard) {
            UnhookWindowsHookEx(keyboard);
        }
        if (mouse) {
            UnhookWindowsHookEx(mouse);
        }
    } };
}

void HotkeyManager::stop_listener()
{
    if (listener_thread_.joinable()) {
        // Wait until the thread has started so we can ask it to quit
        while (listener_thread_id_.load() == 0) {
            std::this_thread::yield();
        }
        PostThreadMessageW(listener_thread_id_.load(), WM_QUIT, 0, 0);
        listener_thread_.join();
    }

    g_manager = nullptr;
}
#else
// Hotkeys need Windows keyboard/mouse hooks; on other systems they're
// saved and shown in the popup but never fire.
void HotkeyManager::start_listener() {}
void HotkeyManager::stop_listener() {}
#endif
}
