#pragma once

#include <SDL3/SDL_rect.h>
#include "game/World.hpp"

#include <cstdint>
#include <vector>

// Positions and blocked cells refer to the player's feet, in logical world units.
class Navigation {
public:
    static constexpr int cellSize = 32;

    Navigation(float width = static_cast<float>(World::width),
               float height = static_cast<float>(World::height));

    void reset(float width, float height, float walkableTopY = 0.0F);
    void setWalkableTop(float y);
    void setBlocked(int column, int row, bool blocked);
    void addObstacle(SDL_FRect bounds, float paddingX, float paddingY);
    [[nodiscard]] std::vector<SDL_FPoint> findPath(SDL_FPoint from, SDL_FPoint to) const;
    [[nodiscard]] std::vector<SDL_FPoint> findPathToNearestReachable(SDL_FPoint from,
                                                                  SDL_FPoint desired) const;
    [[nodiscard]] bool canStandAt(SDL_FPoint point) const;
    [[nodiscard]] int columns() const;
    [[nodiscard]] int rows() const;

private:
    struct Candidate {
        float distanceSquared;
        SDL_FPoint position;
    };

    [[nodiscard]] bool walkable(int column, int row) const;
    [[nodiscard]] bool clearLine(SDL_FPoint from, SDL_FPoint to) const;
    [[nodiscard]] int cellAt(SDL_FPoint point) const;

    float width_ = static_cast<float>(World::width);
    float height_ = static_cast<float>(World::height);
    int columns_ = (World::width + cellSize - 1) / cellSize;
    int rows_ = (World::height + cellSize - 1) / cellSize;
    std::vector<std::uint8_t> blocked_;
    float walkableTopY_ = 0.0F;
    mutable std::vector<float> cost_;
    mutable std::vector<int> parent_;
    mutable std::vector<Candidate> candidates_;
};
