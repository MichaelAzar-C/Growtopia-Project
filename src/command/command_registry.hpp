#pragma once
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "command.hpp"

namespace command {
struct QuickCommandInfo {
    std::string name;
    std::string description;
    bool enabled;
};

class CommandRegistry {
public:
    explicit CommandRegistry(const char prefix = '/')
        : prefix_{ prefix }
    { }

    void set_prefix(char prefix) { prefix_ = prefix; }

    void add(std::unique_ptr<ICommand> cmd);

    template <typename Func>
    void add(const std::string& name, const std::string& description, Func&& func)
    {
        add(make_command(name, description, std::forward<Func>(func)));
    }

    // Quick commands: listed in the /proxy popup (with an on/off tick box)
    // and hidden from /phelp.
    void add_quick(std::unique_ptr<ICommand> cmd, bool enabled = true);
    [[nodiscard]] bool is_quick(std::string_view name) const;
    [[nodiscard]] bool is_enabled(std::string_view name) const;
    void set_enabled(std::string_view name, bool enabled);
    [[nodiscard]] std::vector<QuickCommandInfo> get_quick_commands() const;

    // Hidden commands: always enabled, never listed in /phelp or /proxy.
    void add_hidden(std::unique_ptr<ICommand> cmd);
    [[nodiscard]] bool is_hidden(std::string_view name) const;

    [[nodiscard]] ICommand* get(std::string_view name) const;

    [[nodiscard]] std::vector<std::pair<std::string, std::string>> get_all_commands() const;

    [[nodiscard]] bool is_command(std::string_view input) const;

    [[nodiscard]] bool execute(
        std::string_view input,
        network::Server& server,
        network::Client& client,
        event::Dispatcher& dispatcher,
        std::shared_ptr<core::Scheduler> scheduler
    );

    [[nodiscard]] char prefix() const { return prefix_; }

private:
    [[nodiscard]] std::pair<std::string, std::vector<std::string>> parse(std::string_view input) const;

private:
    char prefix_;
    std::unordered_map<std::string, std::unique_ptr<ICommand>> commands_;
    std::unordered_map<std::string, bool> quick_commands_;
    std::unordered_set<std::string> hidden_commands_;
};
}
