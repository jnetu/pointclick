#pragma once

#include "game/Dialogue.hpp"
#include "game/GameEvent.hpp"
#include "game/Interaction.hpp"
#include "game/Room.hpp"
#include "game/Scene.hpp"
#include "game/ScriptRunner.hpp"
#include "game/SessionState.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Thin coordinator. Owns the progress (SessionState), the room being played (Scene) and what
// runs scripts over both (ScriptRunner, DialoguePlayer, the ScriptHost adapter). Commands come
// in as intents (clicks, item use, dialogue choices); notifications go out as GameEvent.
class Game {
public:
    explicit Game(Room room);

    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    // Content. loadRoom throws on invalid data; applyRoom validates and commits atomically.
    // Edits (resetPlayer = false) preserve feet and SessionState; loads use the spawn.
    void loadRoom(Room room);
    bool applyRoom(Room room, bool resetPlayer, std::string& error);
    void setInteractions(RoomInteractions interactions);
    bool setInteractions(RoomInteractions interactions, std::string& error);
    // The room's dialogue nodes (<room>.dlg). Validated against the room; replacing them ends a
    // conversation in progress. start_dialogue targets are cross-checked by the loader
    // (dialogueReferencesError); an unknown node at runtime just does nothing.
    void setDialogues(RoomDialogues dialogues);
    bool setDialogues(RoomDialogues dialogues, std::string& error);

    // Gameplay and tool commands.
    bool teleportPlayer(SDL_FPoint position, std::string& error);
    bool playObjectAnimation(std::string_view objectId, std::string_view animation,
                             bool restart, std::string& error);
    bool useItemOnObject(std::string_view itemId, std::string_view objectId);
    bool useItemAt(std::string_view itemId, SDL_FPoint worldPoint);
    // Opens a node outside any rule (the same as a rule whose only effect is start_dialogue).
    bool startDialogue(std::string_view nodeId);
    // Picks one of dialogueOptions() while awaitingDialogueChoice(); the reply runs as a sequence.
    bool chooseDialogueOption(std::size_t index);
    void onPointerMove(SDL_FPoint position, bool insideViewport = true);
    void onClick(SDL_FPoint position);
    void tick(float deltaSeconds);

    // State, read-only for graphics, UI and tools.
    [[nodiscard]] const Scene& scene() const { return scene_; }
    [[nodiscard]] SessionState& session() { return session_; }
    [[nodiscard]] const SessionState& session() const { return session_; }
    // A scripted sequence or a conversation holds the scene: world clicks only skip the line.
    [[nodiscard]] bool inCutscene() const;
    [[nodiscard]] bool dialogueActive() const;
    // The menu is up: the node's lines ended and a choice is expected (still inCutscene()).
    [[nodiscard]] bool awaitingDialogueChoice() const;
    [[nodiscard]] std::span<const DialogueChoice* const> dialogueOptions() const;

    // Notifications since the last drain, oldest first. drainEvents swaps them into `out`
    // (cleared first) so a caller-owned buffer is reused frame after frame.
    [[nodiscard]] std::span<const GameEvent> pendingEvents() const { return events_; }
    void drainEvents(std::vector<GameEvent>& out);

private:
    class Host; // ScriptHost adapter over this Game, defined in Game.cpp.

    bool startObjectInteraction(const InteractionTrigger& trigger);
    bool resolveInteraction(const InteractionTrigger& trigger);
    // Applies what scripts deferred (dirty navigation, the room change once they end) and
    // publishes cutscene edges.
    void settleScript();
    void publishCutsceneChange();
    void emit(GameEvent event);

    SessionState session_;
    Scene scene_{session_};
    ScriptRunner script_;
    DialoguePlayer dialogue_;
    std::optional<std::string> scriptRoomTarget_;
    bool navigationDirty_ = false;
    bool wasInCutscene_ = false;
    std::vector<GameEvent> events_;
};
