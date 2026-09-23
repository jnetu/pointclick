#pragma once

#include <SDL3/SDL_rect.h>

#include <cstddef>
#include <span>
#include <vector>

class Player {
public:
    Player(SDL_FPoint start, float speed);

    void setPath(std::vector<SDL_FPoint> path);
    void tick(float deltaSeconds);

    [[nodiscard]] SDL_FPoint feet() const;
    [[nodiscard]] SDL_FPoint interpolatedFeet(float alpha) const;
    [[nodiscard]] SDL_FPoint destination() const;
    [[nodiscard]] std::size_t remainingWaypoints() const;
    [[nodiscard]] std::span<const SDL_FPoint> remainingPath() const;
    [[nodiscard]] bool moving() const;

private:
    float speed_;
    SDL_FPoint previousFeet_;
    SDL_FPoint feet_;
    SDL_FPoint destination_;
    std::vector<SDL_FPoint> path_;
    std::size_t nextWaypoint_ = 0;
};
