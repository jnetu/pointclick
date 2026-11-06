#pragma once

#include "game/Camera.hpp"
#include "game/Dialogue.hpp"
#include "game/FloatingText.hpp"
#include "game/Interaction.hpp"
#include "game/Navigation.hpp"
#include "game/ObjectAnimations.hpp"
#include "game/Player.hpp"
#include "game/Room.hpp"
#include "game/ScriptRunner.hpp"
#include "game/SessionState.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>

class Game {
public:
    explicit Game(Room room);
    void loadRoom(Room room);
    // Validates and commits atomically. Edits preserve feet and SessionState; loads use the spawn.
    bool applyRoom(Room room, bool resetPlayer, std::string& error);
    void setInteractions(RoomInteractions interactions);
    bool setInteractions(RoomInteractions interactions, std::string& error);
    // The room's dialogue nodes (<room>.dlg). Validated against the room; replacing them ends a
    // conversation in progress. start_dialogue targets are cross-checked by the loader
    // (dialogueReferencesError); an unknown node at runtime just does nothing.
    void setDialogues(RoomDialogues dialogues);
    bool setDialogues(RoomDialogues dialogues, std::string& error);
    void refreshNavigation();
    bool teleportPlayer(SDL_FPoint position, std::string& error);
    // Gameplay actions can select and restart one object's animation.
    bool playObjectAnimation(std::string_view objectId, std::string_view animation,
                             bool restart, std::string& error);
    bool useItemOnObject(std::string_view itemId, std::string_view objectId);
    bool useItemAt(std::string_view itemId, SDL_FPoint worldPoint);
    // Opens a node outside any rule (the same as a rule whose only effect is start_dialogue).
    bool startDialogue(std::string_view nodeId);
    // Picks one of dialogueOptions() while awaitingDialogueChoice(); the reply runs as a sequence.
    bool chooseDialogueOption(std::size_t index);

    void onTextInput(std::string_view text);
    void onPointerMove(SDL_FPoint position, bool insideViewport = true);
    void onClick(SDL_FPoint position);
    void tick(float deltaSeconds);

    [[nodiscard]] const FloatingText& floatingText() const;
    [[nodiscard]] const Player& player() const;
    [[nodiscard]] const Camera& camera() const;
    [[nodiscard]] const Navigation& navigation() const;
    [[nodiscard]] SDL_FPoint pointer() const;
    [[nodiscard]] bool pointerInside() const;
    [[nodiscard]] const Room& room() const;
    [[nodiscard]] const RoomInteractions& interactions() const;
    [[nodiscard]] const RoomDialogues& dialogues() const;
    [[nodiscard]] bool dialogueActive() const;
    // The menu is up: the node's lines ended and a choice is expected (still inCutscene()).
    [[nodiscard]] bool awaitingDialogueChoice() const;
    [[nodiscard]] std::span<const DialogueChoice* const> dialogueOptions() const;
    [[nodiscard]] SessionState& session();
    [[nodiscard]] const SessionState& session() const;
    [[nodiscard]] bool isObjectVisible(std::string_view objectId) const;
    [[nodiscard]] bool isObjectSolid(const SceneObject& object) const;
    [[nodiscard]] const std::optional<PendingInteraction>& pendingInteraction() const;
    // A scripted sequence or a conversation holds the scene: world clicks only skip the line.
    [[nodiscard]] bool inCutscene() const;
    [[nodiscard]] const std::optional<std::string>& pendingRoomTransition() const;
    std::optional<std::string> consumeRoomTransition();
    [[nodiscard]] const AnimationPlayback* objectAnimation(std::string_view objectId) const;
    [[nodiscard]] unsigned long long roomRevision() const;

private:
    void buildNavigation(const Room& room, Navigation& target) const;
    [[nodiscard]] static std::optional<SDL_FPoint> findNearestWalkable(
            const Room& room, const Navigation& navigation, SDL_FPoint desiredFeet);
    bool startObjectInteraction(const InteractionTrigger& trigger);
    bool resolveInteraction(const InteractionTrigger& trigger);
    // Applies what the script deferred: dirty navigation and, once it ends, the room change.
    void settleScript();

    class Host; // ScriptHost adapter over this Game, defined in Game.cpp.

    FloatingText floatingText_;
    Room room_;
    RoomInteractions interactions_;
    RoomDialogues dialogues_;
    SessionState session_;
    Navigation navigation_;
    Navigation scratchNavigation_;
    Player player_;
    Camera camera_;
    SDL_FPoint pointer_{};
    bool pointerInside_ = false;
    std::optional<PendingInteraction> pendingInteraction_;
    std::optional<std::string> pendingRoomTransition_;
    ObjectAnimations objectAnimations_;
    ScriptRunner script_;
    DialoguePlayer dialogue_;
    std::optional<std::string> scriptRoomTarget_;
    bool navigationDirty_ = false;
    unsigned long long roomRevision_ = 0;
};
