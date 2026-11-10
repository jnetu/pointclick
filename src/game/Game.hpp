#pragma once

#include "game/Dialogue.hpp"
#include "game/GameEvent.hpp"
#include "game/Interaction.hpp"
#include "game/Room.hpp"
#include "game/SavedGame.hpp"
#include "game/Scene.hpp"
#include "game/ScriptRunner.hpp"
#include "game/SessionState.hpp"

#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Thin coordinator over SessionState, Scene and the script runners. Commands come in as intents
// (clicks, item use, dialogue choices); notifications go out as GameEvent.
class Game {
public:
    explicit Game(Room room);
    // Resumes a save: progress is applied before the room, so navigation already sees it.
    Game(Room room, SessionState progress);

    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    // loadRoom throws on invalid data; applyRoom validates and commits atomically. Edits
    // (resetPlayer = false) keep feet and SessionState; loads enter at spawnId (empty =
    // playerStart), and an unknown spawn refuses the load.
    void loadRoom(Room room);
    bool applyRoom(Room room, bool resetPlayer, std::string& error,
                   std::string_view spawnId = {});
    void setInteractions(RoomInteractions interactions);
    bool setInteractions(RoomInteractions interactions, std::string& error);
    // <room>.dlg nodes, validated against the room; replacing them ends the conversation.
    void setDialogues(RoomDialogues dialogues);
    bool setDialogues(RoomDialogues dialogues, std::string& error);

    // Gameplay and tool commands.
    bool teleportPlayer(SDL_FPoint position, std::string& error);
    // Saved feet: there if walkable, else nearest walkable, else stays. Returns the final point.
    SDL_FPoint placePlayerNear(SDL_FPoint position, std::optional<Facing> facing = std::nullopt);
    bool playObjectAnimation(std::string_view objectId, std::string_view animation,
                             bool restart, std::string& error);
    bool useItemOnObject(std::string_view itemId, std::string_view objectId);
    bool useItemAt(std::string_view itemId, SDL_FPoint worldPoint);
    // Opens a node outside any rule (the same as a rule whose only effect is start_dialogue).
    bool startDialogue(std::string_view nodeId);
    // Picks one of dialogueOptions() while awaitingDialogueChoice(); the reply runs as a sequence.
    bool chooseDialogueOption(std::size_t index);
    // The way to change progress from outside a script (tools, UI, tests): runs `change`, then
    // rebuilds navigation if an object of this room started or stopped blocking the floor, and
    // publishes a cutscene edge.
    void changeSession(const std::function<void(SessionState&)>& change);
    void onPointerMove(SDL_FPoint position, bool insideViewport = true);
    void onClick(SDL_FPoint position);
    void tick(float deltaSeconds);

    // State, read-only for graphics, UI and tools.
    [[nodiscard]] const Scene& scene() const { return scene_; }
    // Read-only on purpose: writes go through changeSession, which keeps navigation in step.
    [[nodiscard]] const SessionState& session() const { return session_; }
    // A scripted sequence or a conversation holds the scene: world clicks only skip the line.
    [[nodiscard]] bool inCutscene() const;
    // For stable moments (!inCutscene()): running scripts are not saved.
    [[nodiscard]] SavedGame savedGame() const;
    [[nodiscard]] bool dialogueActive() const;
    // The menu is up: the node's lines ended and a choice is expected (still inCutscene()).
    [[nodiscard]] bool awaitingDialogueChoice() const;
    [[nodiscard]] std::span<const DialogueChoice* const> dialogueOptions() const;

    // Oldest first; drainEvents swaps into `out` (cleared first) to reuse the caller's buffer.
    [[nodiscard]] std::span<const GameEvent> pendingEvents() const { return events_; }
    void drainEvents(std::vector<GameEvent>& out);

private:
    class Host; // ScriptHost adapter over this Game, defined in Game.cpp.

    bool startObjectInteraction(const InteractionTrigger& trigger);
    bool resolveInteraction(const InteractionTrigger& trigger);
    // Applies what scripts deferred (navigation, room change) and publishes cutscene edges.
    void settleScript();
    void refreshNavigationIfDirty();
    void publishCutsceneChange();
    void emit(GameEvent event);

    SessionState session_;
    Scene scene_{session_};
    ScriptRunner script_;
    DialoguePlayer dialogue_;
    std::optional<RoomChangeRequested> scriptRoomTarget_;
    bool navigationDirty_ = false;
    std::vector<char> solidBeforeChange_; // changeSession scratch, one entry per room object
    bool wasInCutscene_ = false;
    std::vector<GameEvent> events_;
};
