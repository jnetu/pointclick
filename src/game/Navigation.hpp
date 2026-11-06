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
    // One flood fill from `from` picks the reachable cell nearest to `desired`, then one A*
    // (plus a direct attempt when the desired cell itself is reachable).
    [[nodiscard]] std::vector<SDL_FPoint> findPathToNearestReachable(SDL_FPoint from,
                                                                  SDL_FPoint desired) const;
    [[nodiscard]] bool canStandAt(SDL_FPoint point) const;
    [[nodiscard]] int columns() const;
    [[nodiscard]] int rows() const;

private:
    static constexpr int kMoves[8][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1},
                                         {-1, -1}, {-1, 1}, {1, -1}, {1, 1}};

    [[nodiscard]] bool walkable(int column, int row) const;
    // Diagonal moves need both side cells free so the feet never cut a blocked corner.
    [[nodiscard]] bool canStep(int column, int row, const int (&move)[2]) const;
    // Where the feet stand inside a cell (its centre, clamped to the walkable band).
    [[nodiscard]] SDL_FPoint cellPoint(int cell) const;
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
    mutable std::vector<std::uint8_t> reached_;
    mutable std::vector<int> frontier_;
};
