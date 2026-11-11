#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>

// What `pointclick [opcoes]` asked for. Parsed without SDL so it can be tested on its own.
struct CommandLineOptions {
    std::filesystem::path roomsDirectory; // --rooms <pasta>; empty = <base>/assets/rooms
    bool newGame = false;                 // --new-game: do not resume the save
    bool noSave = false;                  // --no-save: never write the autosave
    bool check = false;                   // --check: validate the content and exit, no window
    std::string startRoom;                // --room <id> [spawn]: new game in <id>.room
    std::string startSpawn;

    // --room starts from scratch too: a saved progress belongs to another starting point.
    [[nodiscard]] bool resumesSave() const { return !newGame && startRoom.empty(); }
};

struct CommandLineResult {
    std::optional<CommandLineOptions> options;
    std::string error; // set when options is empty; print it with commandLineUsage()
};

// args excludes the program name (argv[1..]). UTF-8, as SDL hands paths around.
[[nodiscard]] CommandLineResult parseCommandLine(std::span<const std::string_view> args);
[[nodiscard]] std::string_view commandLineUsage();
