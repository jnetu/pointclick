#pragma once

#include "game/Animation.hpp"
#include "game/Facing.hpp"
#include <SDL3/SDL_rect.h>

#include <array>
#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

enum class PlayerPose { idle, left, right, up, down };

// The names the current movement rules require in every room's animation set.
inline constexpr std::array<std::string_view, 5> playerLocomotionAnimations{
    "idle", "left", "right", "up", "down"
};
static_assert(static_cast<std::size_t>(PlayerPose::down) + 1
              == playerLocomotionAnimations.size());

// Optional standing clips per facing, in Facing order; "idle" stands in for a missing one.
inline constexpr std::array<std::string_view, 4> playerIdleAnimations{
    "idle_left", "idle_right", "idle_up", "idle_down"
};

class Player {
public:
    explicit Player(SDL_FPoint start, Facing facing = Facing::down);

    void setPath(std::vector<SDL_FPoint> path);
    void tick(float deltaSeconds, float speed);

    [[nodiscard]] SDL_FPoint feet() const;
    [[nodiscard]] SDL_FPoint interpolatedFeet(float alpha) const;
    [[nodiscard]] SDL_FPoint destination() const;
    [[nodiscard]] std::size_t remainingWaypoints() const;
    [[nodiscard]] std::span<const SDL_FPoint> remainingPath() const;
    [[nodiscard]] bool moving() const;
    [[nodiscard]] PlayerPose pose() const;
    // Set by the spawn and by every step; standing still keeps the last one.
    [[nodiscard]] Facing facing() const;
    [[nodiscard]] std::string_view animationName() const;
    [[nodiscard]] float animationTime() const;
    [[nodiscard]] float interpolatedAnimationTime(float alpha) const;

private:
    SDL_FPoint previousFeet_;
    SDL_FPoint feet_;
    SDL_FPoint destination_;
    std::vector<SDL_FPoint> path_;
    std::size_t nextWaypoint_ = 0;
    PlayerPose pose_ = PlayerPose::idle;
    Facing facing_ = Facing::down;
    AnimationPlayback animation_;
};

// The clip to draw: the locomotion clip, or idle_<facing> when standing and the set has it.
[[nodiscard]] std::string_view playerClipName(const Player& player, const AnimationSet& animations);
