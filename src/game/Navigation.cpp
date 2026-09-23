#include "game/Navigation.hpp"

#include "game/World.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>

namespace {
float distance(const SDL_FPoint a, const SDL_FPoint b) {
    return std::hypot(a.x - b.x, a.y - b.y);
}
}

void Navigation::setWalkableTop(const float y) {
    walkableTopY_ = y;
}

void Navigation::setBlocked(const int column, const int row, const bool blocked) {
    if (column >= 0 && column < columns && row >= 0 && row < rows) {
        blocked_[row * columns + column] = blocked;
    }
}

void Navigation::addObstacle(const SDL_FRect bounds, const float paddingX,
                             const float paddingY) {
    const SDL_FRect padded{bounds.x - paddingX, bounds.y - paddingY,
                           bounds.w + paddingX * 2.0F, bounds.h + paddingY * 2.0F};
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            const SDL_FRect cell{static_cast<float>(column * cellSize),
                                 static_cast<float>(row * cellSize),
                                 static_cast<float>(cellSize),
                                 static_cast<float>(cellSize)};
            if (SDL_HasRectIntersectionFloat(&padded, &cell)) {
                setBlocked(column, row, true);
            }
        }
    }
}

bool Navigation::walkable(const int column, const int row) const {
    return column >= 0 && column < columns && row >= 0 && row < rows
           && (row + 1) * cellSize > walkableTopY_
           && !blocked_[row * columns + column];
}

bool Navigation::canStandAt(const SDL_FPoint point) const {
    if (point.x < 0.0F || point.x >= World::width
        || point.y < walkableTopY_ || point.y >= World::height) {
        return false;
    }
    const int cell = cellAt(point);
    return walkable(cell % columns, cell / columns);
}

int Navigation::cellAt(const SDL_FPoint point) const {
    const int column = std::clamp(static_cast<int>(point.x) / cellSize, 0, columns - 1);
    const int row = std::clamp(static_cast<int>(point.y) / cellSize, 0, rows - 1);
    return row * columns + column;
}

bool Navigation::clearLine(const SDL_FPoint from, const SDL_FPoint to) const {
    const float length = distance(from, to);
    const int steps = std::max(1, static_cast<int>(std::ceil(length / (cellSize * 0.25F))));
    for (int step = 0; step <= steps; ++step) {
        const float t = static_cast<float>(step) / static_cast<float>(steps);
        const SDL_FPoint point{std::lerp(from.x, to.x, t), std::lerp(from.y, to.y, t)};
        const int cell = cellAt(point);
        if (point.y < walkableTopY_ || !walkable(cell % columns, cell / columns)) {
            return false;
        }
    }
    return true;
}

std::vector<SDL_FPoint> Navigation::findPath(const SDL_FPoint from, const SDL_FPoint to) const {
    const int start = cellAt(from);
    const int goal = cellAt(to);
    if (from.y < walkableTopY_ || to.y < walkableTopY_
        || !walkable(start % columns, start / columns)
        || !walkable(goal % columns, goal / columns)) {
        return {};
    }
    if (clearLine(from, to)) {
        return {to};
    }

    struct Entry {
        float estimate;
        int cell;
        bool operator>(const Entry& other) const { return estimate > other.estimate; }
    };

    const auto center = [this](const int cell) -> SDL_FPoint {
        return {static_cast<float>((cell % columns) * cellSize) + cellSize * 0.5F,
                std::clamp(static_cast<float>((cell / columns) * cellSize) + cellSize * 0.5F,
                           walkableTopY_, static_cast<float>(World::height))};
    };

    std::priority_queue<Entry, std::vector<Entry>, std::greater<>> open;
    std::vector<float> cost(columns * rows, std::numeric_limits<float>::infinity());
    std::vector<int> parent(columns * rows, -1);
    cost[start] = 0.0F;
    open.push({distance(center(start), center(goal)), start});

    constexpr int offsets[8][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1},
                                    {-1, -1}, {-1, 1}, {1, -1}, {1, 1}};
    while (!open.empty()) {
        const Entry current = open.top();
        open.pop();
        if (current.cell == goal) {
            break;
        }
        if (current.estimate > cost[current.cell] + distance(center(current.cell), center(goal)) + 0.001F) {
            continue;
        }
        const int x = current.cell % columns;
        const int y = current.cell / columns;
        for (const auto& offset : offsets) {
            const int nx = x + offset[0];
            const int ny = y + offset[1];
            if (!walkable(nx, ny)) {
                continue;
            }
            if (offset[0] != 0 && offset[1] != 0
                && (!walkable(x + offset[0], y) || !walkable(x, y + offset[1]))) {
                continue;
            }
            const int next = ny * columns + nx;
            const float nextCost = cost[current.cell] + distance(center(current.cell), center(next));
            if (nextCost < cost[next]) {
                cost[next] = nextCost;
                parent[next] = current.cell;
                open.push({nextCost + distance(center(next), center(goal)), next});
            }
        }
    }

    if (start != goal && parent[goal] == -1) {
        return {};
    }

    std::vector<SDL_FPoint> reversed;
    for (int cell = goal; cell != start; cell = parent[cell]) {
        reversed.push_back(center(cell));
    }
    std::reverse(reversed.begin(), reversed.end());
    reversed.push_back(to);

    // Remove cell-by-cell turns when the straight segment is free.
    std::vector<SDL_FPoint> path;
    SDL_FPoint cursor = from;
    for (std::size_t first = 0; first < reversed.size();) {
        std::size_t furthest = first;
        for (std::size_t candidate = first + 1; candidate < reversed.size(); ++candidate) {
            if (clearLine(cursor, reversed[candidate])) {
                furthest = candidate;
            }
        }
        path.push_back(reversed[furthest]);
        cursor = reversed[furthest];
        first = furthest + 1;
    }
    return path;
}

std::vector<SDL_FPoint> Navigation::findPathToNearestReachable(
        const SDL_FPoint from, const SDL_FPoint desired) const {
    if (const auto direct = findPath(from, desired); !direct.empty()) {
        return direct;
    }

    struct Candidate {
        float distanceSquared;
        SDL_FPoint position;
    };
    std::vector<Candidate> candidates;
    candidates.reserve(columns * rows);
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            if (!walkable(column, row)) {
                continue;
            }
            const SDL_FPoint position{column * cellSize + cellSize * 0.5F,
                                      std::clamp(row * cellSize + cellSize * 0.5F,
                                                 walkableTopY_,
                                                 static_cast<float>(World::height - 1))};
            const float dx = position.x - desired.x;
            const float dy = position.y - desired.y;
            candidates.push_back({dx * dx + dy * dy, position});
        }
    }
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        return a.distanceSquared < b.distanceSquared;
    });
    for (const Candidate& candidate : candidates) {
        if (const auto route = findPath(from, candidate.position); !route.empty()) {
            return route;
        }
    }
    return {};
}
