#pragma once

#include "game/Animation.hpp"
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

class Player {
public:
    explicit Player(SDL_FPoint start);

    void setPath(std::vector<SDL_FPoint> path);
    void tick(float deltaSeconds, float speed);

    [[nodiscard]] SDL_FPoint feet() const;
    [[nodiscard]] SDL_FPoint interpolatedFeet(float alpha) const;
    [[nodiscard]] SDL_FPoint destination() const;
    [[nodiscard]] std::size_t remainingWaypoints() const;
    [[nodiscard]] std::span<const SDL_FPoint> remainingPath() const;
    [[nodiscard]] bool moving() const;
    [[nodiscard]] PlayerPose pose() const;
    [[nodiscard]] std::string_view animationName() const;
    [[nodiscard]] float animationTime() const;

private:
    SDL_FPoint previousFeet_;
    SDL_FPoint feet_;
    SDL_FPoint destination_;
    std::vector<SDL_FPoint> path_;
    std::size_t nextWaypoint_ = 0;
    PlayerPose pose_ = PlayerPose::idle;
    AnimationPlayback animation_;
};
