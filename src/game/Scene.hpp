#pragma once

#include "game/Camera.hpp"
#include "game/Dialogue.hpp"
#include "game/FloatingText.hpp"
#include "game/Interaction.hpp"
#include "game/Navigation.hpp"
#include "game/ObjectAnimations.hpp"
#include "game/Player.hpp"
#include "game/Room.hpp"

#include <optional>
#include <string>
#include <string_view>

class SessionState;

// The room being played: its loaded content (Room, rules, dialogue nodes) and the local
// simulation that goes with it (navigation, player, camera, object animations, the line on
// screen, walk-to-interact). Nothing here outlives a room change except what SessionState keeps.
// The scene only reads the session (visibility, solidity, inventory for use_item); changes to
// it come from scripts, coordinated by Game.
class Scene {
public:
    explicit Scene(const SessionState& session);

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    // Validates and commits atomically: nothing changes when it returns false. resetPlayer
    // places the feet at the room's entry point (spawnId, or playerStart when empty; an
    // unknown spawn is an error) and snaps the camera; otherwise feet, facing and object
    // animations survive.
    bool apply(Room room, bool resetPlayer, std::string& error, std::string_view spawnId = {});
    bool setInteractions(RoomInteractions interactions, std::string& error);
    bool setDialogues(RoomDialogues dialogues, std::string& error);
    // Rules and dialogue nodes belong to one room; called when another room is entered.
    void clearContent();

    void refreshNavigation();
    bool teleportPlayer(SDL_FPoint position, std::string& error);
    // teleportPlayer that never fails: blocked = nearest walkable point; none = stay put.
    // facing: the saved one; nullopt keeps the current one.
    SDL_FPoint placePlayerNear(SDL_FPoint position, std::optional<Facing> facing = std::nullopt);
    bool playObjectAnimation(std::string_view objectId, std::string_view animation,
                             bool restart, std::string& error);

    // Walk-to-interact for a click or an item use. arrived: resolve the trigger now;
    // walking: pendingInteraction() is set and tick() returns the trigger on arrival.
    enum class Approach { failed, walking, arrived };
    Approach approach(const InteractionTrigger& trigger);
    // The target is still visible and, for use_item, the item is still held.
    [[nodiscard]] bool canResolve(const InteractionTrigger& trigger) const;
    // Scripted walk_to: false when already there, hidden or unreachable.
    bool walkToObject(std::string_view objectId);
    void walkToFloor(SDL_FPoint position);
    // Stops the walk and drops any pending interaction.
    void stopPlayer();
    [[nodiscard]] const SceneObject* pick(SDL_FPoint worldPoint) const;
    void onPointerMove(SDL_FPoint position, bool insideViewport);

    FloatingText& speech() { return speech_; }

    // Advances the local simulation; returns the trigger whose approach walk just ended.
    std::optional<InteractionTrigger> tick(float deltaSeconds);
    // Camera last, after scripts may have moved or teleported the player this tick.
    void followPlayer(float deltaSeconds);

    [[nodiscard]] const Room& room() const { return room_; }
    [[nodiscard]] const RoomInteractions& interactions() const { return interactions_; }
    [[nodiscard]] const RoomDialogues& dialogues() const { return dialogues_; }
    [[nodiscard]] const Player& player() const { return player_; }
    [[nodiscard]] const Camera& camera() const { return camera_; }
    [[nodiscard]] const Navigation& navigation() const { return navigation_; }
    [[nodiscard]] const FloatingText& floatingText() const { return speech_; }
    [[nodiscard]] SDL_FPoint pointer() const { return pointer_; }
    [[nodiscard]] bool pointerInside() const { return pointerInside_; }
    [[nodiscard]] const std::optional<PendingInteraction>& pendingInteraction() const {
        return pendingInteraction_;
    }
    [[nodiscard]] bool isObjectVisible(std::string_view objectId) const;
    [[nodiscard]] bool isObjectSolid(const SceneObject& object) const;
    [[nodiscard]] const AnimationPlayback* objectAnimation(std::string_view objectId) const;
    // True when the object's current clip ended, loops forever, or does not exist.
    [[nodiscard]] bool animationFinished(std::string_view objectId) const;
    // Bumped on every committed apply(); renderers compare it to notice definition changes.
    [[nodiscard]] unsigned long long roomRevision() const { return roomRevision_; }

private:
    void buildNavigation(const Room& room, Navigation& target) const;
    [[nodiscard]] static std::optional<SDL_FPoint> findNearestWalkable(
            const Room& room, const Navigation& navigation, SDL_FPoint desiredFeet);

    const SessionState& session_;
    Room room_;
    RoomInteractions interactions_;
    RoomDialogues dialogues_;
    Navigation navigation_;
    Navigation scratchNavigation_;
    Player player_;
    Camera camera_;
    ObjectAnimations objectAnimations_;
    FloatingText speech_;
    SDL_FPoint pointer_{};
    bool pointerInside_ = false;
    std::optional<PendingInteraction> pendingInteraction_;
    unsigned long long roomRevision_ = 0;
};
