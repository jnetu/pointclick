#pragma once

#include <span>
#include <sstream>
#include <string>
#include <string_view>

class CommandConsole;
class Game;
struct CommandDescriptor;

// What a command gets: the game, the tools' shared state, the arguments after the verb (classic
// locale) and the text it answers with.
struct CommandContext {
    Game& game;
    CommandConsole& console;
    std::istringstream& args;
    std::string& output;
    const CommandDescriptor& command;

    // "Use: <usage>" from the catalog entry; returns false so a command can `return usage();`.
    bool usage() const;
    // Sets the error text; returns false.
    bool fail(std::string text) const;
    // Sets the confirmation text; returns true.
    bool done(std::string text) const;
};

// One command of the tools (DEBUG panel and --exec). `help`, the panel's command list and the
// documentation come from this table: a new command is one entry + one function in
// CommandCatalog.cpp. run returns false when the command failed (the headless runner then exits
// with an error).
struct CommandDescriptor {
    std::string_view name;
    std::string_view usage;       // "flag <id> <n>": the arguments, as the "Use:" message shows
    std::string_view description; // one line, ASCII (the panel draws with the debug font)
    bool (*run)(CommandContext& context);
};

[[nodiscard]] std::span<const CommandDescriptor> commandCatalog();
[[nodiscard]] const CommandDescriptor* findCommand(std::string_view name);
// Every command name, space-separated, in catalog order (the panel's list).
[[nodiscard]] std::string commandNames();
