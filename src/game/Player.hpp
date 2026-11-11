#pragma once

#include "game/Animation.hpp"
#include "game/Facing.hpp"
#include "game/PlayerClips.hpp"
#include <SDL3/SDL_rect.h>

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

enum class PlayerPose { idle, left, right, up, down };

// One locomotion clip name per pose (game/PlayerClips.hpp).
static_assert(static_cast<std::size_t>(PlayerPose::down) + 1
              == playerLocomotionAnimations.size());

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
    // Turns in place (scripted `face`); the next step of a walk sets it again.
    void face(Facing facing);
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
