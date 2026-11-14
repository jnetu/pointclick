#pragma once

#include "game/Dialogue.hpp"
#include "game/Interaction.hpp"
#include "game/Room.hpp"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

class Game;
class RoomLibrary;

// What the tools draw over the game, switched with `show <view> <0|1>`. None of it exists in a
// build without tools, and with the editor closed only the hint is drawn.
struct DebugViews {
    bool route = true;       // route and destination cross, while the editor is active
    bool diagnostics = true; // technical readout, while the editor is active
    bool hint = true;        // how to open the editor, while it is closed
};

// One command line run against the game: ok = the command did what it says; output is what
// the panel shows as status and what the headless runner prints.
struct CommandResult {
    bool ok = true;
    std::string output;
};

// The command side of the tools, without input or drawing: the state the commands share (the
// room library, the art reloader, the undo and reset points, the debug views, where a new game
// starts) and execute(), which looks the verb up in debug/CommandCatalog. DebugEditor feeds it
// the typed line; the headless runner (debug/CommandScript, --exec) feeds it lines from the
// command line. A new command is an entry + a function in CommandCatalog.cpp, never a change
// here.
class CommandConsole {
public:
    CommandConsole();

    // save/load/goto go through the same library the game uses. It must outlive the console.
    void setLibrary(const RoomLibrary& library) { library_ = &library; }
    // Called by `load` and `reload` so PNGs changed on disk are read again; unset, `reload`
    // reports it as unavailable and `load` only skips it.
    void setArtReloader(std::function<void()> reloadArt) { reloadArt_ = std::move(reloadArt); }
    // Where `newgame` starts (the --room <sala> [spawn] of the run; default first.room).
    void setStartRoom(std::string roomId, std::string spawnId = {});

    // The room `reset` returns to is the one in play now; `undo` starts empty.
    void begin(const Game& game);
    // Runs one line ("verb args"). A blank line is ok with no output.
    CommandResult execute(std::string_view line, Game& game);

    // For the catalog's commands.
    [[nodiscard]] const RoomLibrary* library() const { return library_; }
    // false when no reloader is wired (headless runs, tests).
    bool reloadArt() const;
    [[nodiscard]] DebugViews& views() { return views_; }
    [[nodiscard]] const DebugViews& views() const { return views_; }
    [[nodiscard]] const std::string& startRoom() const { return startRoom_; }
    [[nodiscard]] const std::string& startSpawn() const { return startSpawn_; }
    // Validates and applies an edited room (and, when given, the rules and dialogues that go
    // with it); keeps the previous one for `undo`. output gets the error or the confirmation.
    bool commit(Game& game, Room room, std::string& output, bool resetPlayer = false,
                std::optional<RoomInteractions> nextInteractions = std::nullopt,
                std::optional<RoomDialogues> nextDialogues = std::nullopt,
                std::string_view spawnId = {});
    // A RoomParameters entry by key, range-checked, through commit.
    bool setParameter(Game& game, std::string_view key, float value, std::string& output);
    bool undo(Game& game, std::string& output);
    bool reset(Game& game, std::string& output);

private:
    const RoomLibrary* library_ = nullptr;
    std::function<void()> reloadArt_;
    DebugViews views_;
    std::string startRoom_;
    std::string startSpawn_;
    std::optional<Room> baseline_;
    std::optional<RoomInteractions> baselineInteractions_;
    std::optional<RoomDialogues> baselineDialogues_;
    std::optional<Room> undo_;
    std::optional<RoomInteractions> undoInteractions_;
    std::optional<RoomDialogues> undoDialogues_;
};
