#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// One --exec <comando> or --exec-file <arquivo>, in command-line order.
struct ExecStep {
    std::string command;        // --exec
    std::filesystem::path file; // --exec-file (UTF-8 argument)
};

// What `pointclick [opcoes]` asked for. Parsed without SDL so it can be tested on its own.
struct CommandLineOptions {
    std::filesystem::path roomsDirectory; // --rooms <pasta>; empty = <base>/assets/rooms
    bool newGame = false;                 // --new-game: do not resume the save
    bool noSave = false;                  // --no-save: never write the autosave
    bool check = false;                   // --check: validate the content and exit, no window
    std::string startRoom;                // --room <id> [spawn]: new game in <id>.room
    std::string startSpawn;
    // --exec / --exec-file (builds with tools only): run DEBUG commands with no window, on a new
    // game that never touches the save, and exit.
    std::vector<ExecStep> exec;

    // --room starts from scratch too: a saved progress belongs to another starting point.
    [[nodiscard]] bool resumesSave() const { return !newGame && startRoom.empty(); }
    [[nodiscard]] bool runsCommands() const { return !exec.empty(); }
};

struct CommandLineResult {
    std::optional<CommandLineOptions> options;
    std::string error; // set when options is empty; print it with commandLineUsage()
};

// args excludes the program name (argv[1..]). UTF-8, as SDL hands paths around.
[[nodiscard]] CommandLineResult parseCommandLine(std::span<const std::string_view> args);
[[nodiscard]] std::string_view commandLineUsage();
