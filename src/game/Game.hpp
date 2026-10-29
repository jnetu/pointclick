#pragma once

#include "game/Camera.hpp"
#include "game/FloatingText.hpp"
#include "game/Interaction.hpp"
#include "game/Navigation.hpp"
#include "game/ObjectAnimations.hpp"
#include "game/Player.hpp"
#include "game/Room.hpp"
#include "game/SessionState.hpp"

#include <optional>
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
    void refreshNavigation();
    bool teleportPlayer(SDL_FPoint position, std::string& error);
    // Gameplay actions can select and restart one object's animation.
    bool playObjectAnimation(std::string_view objectId, std::string_view animation,
                             bool restart, std::string& error);
    bool useItemOnObject(std::string_view itemId, std::string_view objectId);
    bool useItemAt(std::string_view itemId, SDL_FPoint worldPoint);

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
    [[nodiscard]] SessionState& session();
    [[nodiscard]] const SessionState& session() const;
    [[nodiscard]] bool isObjectVisible(std::string_view objectId) const;
    [[nodiscard]] bool isObjectSolid(const SceneObject& object) const;
    [[nodiscard]] const std::optional<PendingInteraction>& pendingInteraction() const;
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

    FloatingText floatingText_;
    Room room_;
    RoomInteractions interactions_;
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
    unsigned long long roomRevision_ = 0;
};
