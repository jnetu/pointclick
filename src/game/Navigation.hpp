#pragma once

#include <SDL3/SDL_rect.h>

#include <vector>

// Positions and blocked cells refer to the player's feet, in logical world units.
class Navigation {
public:
    static constexpr int cellSize = 32;

    void setWalkableTop(float y);
    void setBlocked(int column, int row, bool blocked);
    void addObstacle(SDL_FRect bounds, float paddingX, float paddingY);
    [[nodiscard]] std::vector<SDL_FPoint> findPath(SDL_FPoint from, SDL_FPoint to) const;
    [[nodiscard]] std::vector<SDL_FPoint> findPathToNearestReachable(SDL_FPoint from,
                                                                     SDL_FPoint desired) const;
    [[nodiscard]] bool canStandAt(SDL_FPoint point) const;

private:
    static constexpr int columns = 40;
    static constexpr int rows = 23;

    [[nodiscard]] bool walkable(int column, int row) const;
    [[nodiscard]] bool clearLine(SDL_FPoint from, SDL_FPoint to) const;
    [[nodiscard]] int cellAt(SDL_FPoint point) const;

    std::vector<bool> blocked_ = std::vector<bool>(columns * rows, false);
    float walkableTopY_ = 0.0F;
};
