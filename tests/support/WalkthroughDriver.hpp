#pragma once

#include "content/RoomLibrary.hpp"
#include "game/Autosave.hpp"
#include "game/Facing.hpp"
#include "game/Game.hpp"
#include "game/SavedGame.hpp"
#include "support/TempDirectory.hpp"

#include <cstddef>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// What a failed step throws: doctest reports it with the whole message (the step, what was obtained
// against what was expected, the state and the steps before). Tests of the driver itself catch it.
class WalkthroughFailure : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Plays the shipped rooms (assets/rooms, never fixtures) with no window and no real clock: the
// headless "walkthrough" behind the playground tests (docs/ROADMAP.md, F3).
//
// It does what the shell does around a Game and nothing more: starts a new game like
// `pointclick --room <sala> [spawn] --new-game --no-save`, ticks at the fixed 60 Hz of
// Application, answers RoomChangeRequested by loading the room (RoomLibrary + enterRoom) and runs
// the AutosavePolicy on RoomEntered/CutsceneEnded. Time is simulated: a tick is exactly 1/60 s,
// so a run always gives the same result. It links `game` and `content` only, so the store build
// runs it too; a test that wants pixels renders driver.game() with graphics/ itself.
//
// A walkthrough is a sequence of calls, each one a numbered step:
//
//   WalkthroughDriver play("lever");
//   play.clickObject("lever").settle();                  // click, walk, run the whole script
//   play.expectRoom("lever").expectFlag("lever_pulled", 1).expectFacing(Facing::right);
//
// Every call that fails (an expectation, a click the game refused, a script that never ends)
// stops the test case with the step number and what it did, the state obtained against the one
// expected, and the last steps before it.
class WalkthroughDriver {
public:
    static constexpr float tickSeconds = 1.0F / 60.0F;
    // The safety limit of the waits: a script that outlasts it is a softlock, reported as one.
    static constexpr float defaultLimitSeconds = 120.0F;

    // The shipped <repo>/assets/rooms, a new game in `room` at `spawn` (empty = playerStart).
    explicit WalkthroughDriver(std::string_view room, std::string_view spawn = {});
    // The same over another rooms folder (a fixture), given last so a literal never picks it.
    WalkthroughDriver(std::string_view room, std::string_view spawn,
                      std::filesystem::path roomsDirectory);

    WalkthroughDriver(const WalkthroughDriver&) = delete;
    WalkthroughDriver& operator=(const WalkthroughDriver&) = delete;

    [[nodiscard]] Game& game() { return *game_; }
    [[nodiscard]] const Game& game() const { return *game_; }
    [[nodiscard]] const RoomLibrary& library() const { return library_; }
    // Simulated time since the driver was built (ticks / 60).
    [[nodiscard]] double seconds() const { return static_cast<double>(ticks_) * tickSeconds; }

    // ---- Input: what the player does ----
    // A click at a viewport point (0..1280, 0..720), converted with the camera exactly as
    // InputRouter does. Under a scripted cutscene it only skips the line, like in the game.
    WalkthroughDriver& click(float viewportX, float viewportY);
    // The middle of the object's bounds, as a viewport click (the object must be on screen).
    WalkthroughDriver& clickObject(std::string_view objectId);
    // A click anywhere: during a cutscene it skips the line being spoken.
    WalkthroughDriver& skipLine();
    // Picks a dialogue option by its choice id (the menu must be up: see waitForChoice).
    WalkthroughDriver& choose(std::string_view choiceId);
    // The item must be in the inventory; the player walks to the object and the rule runs.
    WalkthroughDriver& use(std::string_view itemId, std::string_view objectId);

    // ---- Session commands (what the F2 editor commands do, through the same Game doors) ----
    WalkthroughDriver& give(std::string_view itemId);
    WalkthroughDriver& setFlag(std::string_view flagId, int value);
    // Like `goto`: asks for the room change the way change_room does; progress is kept.
    WalkthroughDriver& goTo(std::string_view roomId, std::string_view spawnId = {});

    // ---- Time ----
    // A fixed number of ticks / of simulated seconds (rounded up to whole ticks).
    WalkthroughDriver& advanceTicks(int ticks);
    WalkthroughDriver& advance(float seconds);
    // Until the scene is idle: no script or conversation running, the player standing and no
    // walk-to-interact pending. Fails at the limit (a softlock) and when a dialogue menu is up
    // (nothing would ever end it: choose() first).
    WalkthroughDriver& settle(float limitSeconds = defaultLimitSeconds);
    // Until the dialogue menu is up; fails if the conversation ends or the limit passes first.
    WalkthroughDriver& waitForChoice(float limitSeconds = defaultLimitSeconds);
    // Until `done()` holds; `what` names the wait in the failure.
    WalkthroughDriver& waitUntil(std::string_view what, const std::function<bool()>& done,
                                 float limitSeconds = defaultLimitSeconds);

    // ---- Save: stops at a stable moment, writes save.slot and resumes it in a new Game ----
    // The same path as `pointclick` resuming a save (SaveFiles::save, then resume + the progress
    // applied before the room). Recorded lines, rooms and autosaves are kept.
    WalkthroughDriver& saveAndResume();

    // ---- Expectations (each one a step; a failure shows obtained against expected) ----
    WalkthroughDriver& expectRoom(std::string_view roomId);
    WalkthroughDriver& expectFeet(float x, float y, float tolerance = 2.0F);
    WalkthroughDriver& expectFacing(Facing facing);
    WalkthroughDriver& expectFlag(std::string_view flagId, int value);
    WalkthroughDriver& expectItems(std::initializer_list<std::string_view> items);
    WalkthroughDriver& expectObjectVisible(std::string_view objectId, bool visible);
    WalkthroughDriver& expectObjectSolid(std::string_view objectId, bool solid);
    // The clip the object is playing now; finished = its last frame (or a still image).
    WalkthroughDriver& expectObjectAnimation(std::string_view objectId, std::string_view clip,
                                             bool finished);
    WalkthroughDriver& expectCameraX(float x, float tolerance = 0.5F);
    WalkthroughDriver& expectIdle();
    // The lines spoken since the last clearLines(), in order / the most recent one.
    WalkthroughDriver& expectLines(std::initializer_list<std::string_view> lines);
    WalkthroughDriver& expectLastLine(std::string_view line);
    // Options of the menu now up, by choice id, in menu order.
    WalkthroughDriver& expectChoices(std::initializer_list<std::string_view> choiceIds);
    // How many rooms the shell has loaded since the start (the first one included) and the
    // autosave writes: one per room entered, the last one holding the stable state of it.
    WalkthroughDriver& expectRoomsEntered(std::size_t count);
    WalkthroughDriver& expectLastAutosave(std::string_view roomId, float x, float y, Facing facing,
                                          float tolerance = 2.0F);
    WalkthroughDriver& clearLines();

    [[nodiscard]] const std::vector<std::string>& lines() const { return lines_; }
    [[nodiscard]] const std::vector<SavedGame>& autosaves() const { return autosaves_; }
    [[nodiscard]] const std::vector<RoomEntered>& roomsEntered() const { return entered_; }

    // A one-line picture of the state, as the failures print it.
    [[nodiscard]] std::string describeState() const;

private:
    void begin(std::string description);
    [[noreturn]] void fail(const std::string& detail);
    template <typename T>
    void expectEqual(std::string_view what, const T& actual, const T& expected);
    void startGame(RoomBundle bundle, const SavedGame* save, std::string_view spawnId);
    void tick();
    void processEvents();
    [[nodiscard]] bool idle() const;
    [[nodiscard]] std::string choiceIds() const;

    RoomLibrary library_;
    std::unique_ptr<Game> game_;
    AutosavePolicy autosave_;
    std::vector<std::string> lines_;
    std::vector<RoomEntered> entered_;
    std::vector<SavedGame> autosaves_;
    std::vector<GameEvent> eventBuffer_;
    long long ticks_ = 0;
    std::unique_ptr<TempDirectory> saveFolder_;
    std::vector<std::string> trail_; // every step, "N) what it did"
};
