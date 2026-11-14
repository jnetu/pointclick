#pragma once

#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

class CommandConsole;
class Game;
class RoomLibrary;

// Where a headless command (--exec "<comando>" or --exec-file <arquivo>) comes from, in order.
struct CommandSource {
    std::string command;        // one command line, or
    std::filesystem::path file; // a file with one command per line ('#' comments, blank lines)
};

// One line of the run's report; the executable prints each to stdout as it comes.
using CommandPrinter = std::function<void(std::string_view line)>;

// The headless side of the tools: the same CommandConsole and catalog as the DEBUG panel,
// driven by a list of lines, with no window and no renderer. After each command the game is
// settled the way the main loop would: fixed 60 Hz ticks until the player stops and no
// sequence or conversation is running (the rule a `use` walks to runs here), loading every
// room a RoomChangeRequested asks for (`goto`, `newgame`, change_room) through enterRoom.
// Spoken lines and entered rooms are reported between the commands.
class CommandScript {
public:
    CommandScript(Game& game, CommandConsole& console, const RoomLibrary& library,
                  CommandPrinter print);

    // Runs one line and settles the game. Prints "> <line>", then the command's output, with
    // each line of a failure prefixed "erro: ". Returns whether the command (and the settling)
    // succeeded.
    bool run(std::string_view line);
    // Ticks and routes the game's events until it is idle. false (and a printed error) when it
    // is still busy after the time limit or a requested room does not load.
    bool settle();

private:
    void printOutput(std::string_view text, bool failed) const;
    bool processEvents();

    Game& game_;
    CommandConsole& console_;
    const RoomLibrary& library_;
    CommandPrinter print_;
};

// Reads --exec-file contents: one command per line; blank lines and '#' comment lines are
// skipped. false with `error` set when a file cannot be read.
[[nodiscard]] bool expandCommandSources(std::span<const CommandSource> sources,
                                        std::vector<std::string>& commands, std::string& error);

// `pointclick --exec ...`: opens the room library, starts a new game in startRoom (empty =
// first.room) at startSpawn, never reads or writes the save, and runs every command, even
// after a failure. Returns the exit code: 0 when every command succeeded, 1 otherwise (also
// when the start room does not load).
[[nodiscard]] int runCommandScript(const std::filesystem::path& roomsDirectory,
                                   std::string_view startRoom, std::string_view startSpawn,
                                   std::span<const CommandSource> sources,
                                   const CommandPrinter& print);
