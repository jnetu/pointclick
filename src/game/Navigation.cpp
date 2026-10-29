#include "game/Navigation.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>

namespace {
float distance(const SDL_FPoint a, const SDL_FPoint b) {
    return std::hypot(a.x - b.x, a.y - b.y);
}
}

Navigation::Navigation(const float width, const float height) {
    reset(width, height);
}

void Navigation::reset(const float width, const float height, const float walkableTopY) {
    width_ = std::max(width, static_cast<float>(cellSize));
    height_ = std::max(height, static_cast<float>(cellSize));
    columns_ = (static_cast<int>(std::ceil(width_)) + cellSize - 1) / cellSize;
    rows_ = (static_cast<int>(std::ceil(height_)) + cellSize - 1) / cellSize;
    walkableTopY_ = walkableTopY;
    blocked_.assign(static_cast<std::size_t>(columns_ * rows_), 0);
}

void Navigation::setWalkableTop(const float y) {
    walkableTopY_ = y;
}

void Navigation::setBlocked(const int column, const int row, const bool blocked) {
    if (column >= 0 && column < columns_ && row >= 0 && row < rows_) {
        blocked_[static_cast<std::size_t>(row * columns_ + column)] = blocked ? 1U : 0U;
    }
}

void Navigation::addObstacle(const SDL_FRect bounds, const float paddingX,
                             const float paddingY) {
    const SDL_FRect padded{bounds.x - paddingX, bounds.y - paddingY,
                           bounds.w + paddingX * 2.0F, bounds.h + paddingY * 2.0F};
    if (padded.w <= 0.0F || padded.h <= 0.0F) {
        return;
    }
    const int minColumn = std::max(0, static_cast<int>(std::floor(padded.x / cellSize)));
    const int maxColumn = std::min(columns_ - 1,
                                   static_cast<int>(std::floor((padded.x + padded.w) / cellSize)));
    const int minRow = std::max(0, static_cast<int>(std::floor(padded.y / cellSize)));
    const int maxRow = std::min(rows_ - 1,
                                static_cast<int>(std::floor((padded.y + padded.h) / cellSize)));
    for (int row = minRow; row <= maxRow; ++row) {
        for (int column = minColumn; column <= maxColumn; ++column) {
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
    return column >= 0 && column < columns_ && row >= 0 && row < rows_
           && (row + 1) * cellSize > walkableTopY_
           && blocked_[static_cast<std::size_t>(row * columns_ + column)] == 0U;
}

bool Navigation::canStandAt(const SDL_FPoint point) const {
    if (!std::isfinite(point.x) || !std::isfinite(point.y)
        || point.x < 0.0F || point.x >= width_
        || point.y < walkableTopY_ || point.y >= height_) {
        return false;
    }
    const int cell = cellAt(point);
    return walkable(cell % columns_, cell / columns_);
}

int Navigation::columns() const {
    return columns_;
}

int Navigation::rows() const {
    return rows_;
}

int Navigation::cellAt(const SDL_FPoint point) const {
    const int column = std::clamp(static_cast<int>(point.x) / cellSize, 0, columns_ - 1);
    const int row = std::clamp(static_cast<int>(point.y) / cellSize, 0, rows_ - 1);
    return row * columns_ + column;
}

bool Navigation::clearLine(const SDL_FPoint from, const SDL_FPoint to) const {
    if (!canStandAt(from) || !canStandAt(to)) return false;
    // Exact segment checks prevent the path smoother from clipping a cell corner.
    const int left = static_cast<int>(std::min(from.x, to.x)) / cellSize;
    const int right = static_cast<int>(std::max(from.x, to.x)) / cellSize;
    const int top = static_cast<int>(std::min(from.y, to.y)) / cellSize;
    const int bottom = static_cast<int>(std::max(from.y, to.y)) / cellSize;
    for (int row = top; row <= bottom; ++row) {
        for (int column = left; column <= right; ++column) {
            if (blocked_[static_cast<std::size_t>(row * columns_ + column)] == 0U) continue;
            const SDL_FRect cell{static_cast<float>(column * cellSize),
                                 static_cast<float>(row * cellSize),
                                 static_cast<float>(cellSize), static_cast<float>(cellSize)};
            float x1 = from.x, y1 = from.y, x2 = to.x, y2 = to.y;
            if (SDL_GetRectAndLineIntersectionFloat(&cell, &x1, &y1, &x2, &y2)) return false;
        }
    }
    return true;
}

std::vector<SDL_FPoint> Navigation::findPath(const SDL_FPoint from, const SDL_FPoint to) const {
    if (!canStandAt(from) || !canStandAt(to)) return {};
    const int start = cellAt(from);
    const int goal = cellAt(to);
    if (from.y < walkableTopY_ || to.y < walkableTopY_
        || !walkable(start % columns_, start / columns_)
        || !walkable(goal % columns_, goal / columns_)) {
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

    const auto center = [this, start, goal, from, to](const int cell) -> SDL_FPoint {
        if (cell == start) return from;
        if (cell == goal) return to;
        return {std::min(static_cast<float>((cell % columns_) * cellSize) + cellSize * 0.5F,
                         width_ - 1.0F),
                std::clamp(static_cast<float>((cell / columns_) * cellSize) + cellSize * 0.5F,
                           walkableTopY_, height_ - 1.0F)};
    };

    const auto totalCells = static_cast<std::size_t>(columns_ * rows_);
    cost_.assign(totalCells, std::numeric_limits<float>::infinity());
    parent_.assign(totalCells, -1);

    std::priority_queue<Entry, std::vector<Entry>, std::greater<>> open;
    cost_[static_cast<std::size_t>(start)] = 0.0F;
    open.push({distance(center(start), center(goal)), start});

    constexpr int offsets[8][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1},
                                    {-1, -1}, {-1, 1}, {1, -1}, {1, 1}};
    while (!open.empty()) {
        const Entry current = open.top();
        open.pop();
        if (current.cell == goal) {
            break;
        }
        const auto currentIndex = static_cast<std::size_t>(current.cell);
        if (current.estimate > cost_[currentIndex] + distance(center(current.cell), center(goal)) + 0.001F) {
            continue;
        }
        const int x = current.cell % columns_;
        const int y = current.cell / columns_;
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
            const int next = ny * columns_ + nx;
            if (!clearLine(center(current.cell), center(next))) continue;
            const float nextCost = cost_[currentIndex] + distance(center(current.cell), center(next));
            const auto nextIndex = static_cast<std::size_t>(next);
            if (nextCost < cost_[nextIndex]) {
                cost_[nextIndex] = nextCost;
                parent_[nextIndex] = current.cell;
                open.push({nextCost + distance(center(next), center(goal)), next});
            }
        }
    }

    if (start != goal && parent_[static_cast<std::size_t>(goal)] == -1) {
        return {};
    }

    std::vector<SDL_FPoint> reversed;
    for (int cell = goal; cell != start; cell = parent_[static_cast<std::size_t>(cell)]) {
        reversed.push_back(center(cell));
    }
    std::reverse(reversed.begin(), reversed.end());

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
    if (!canStandAt(from) || !std::isfinite(desired.x) || !std::isfinite(desired.y)) return {};
    if (const auto direct = findPath(from, desired); !direct.empty()) {
        return direct;
    }

    candidates_.clear();
    candidates_.reserve(static_cast<std::size_t>(columns_ * rows_));
    for (int row = 0; row < rows_; ++row) {
        for (int column = 0; column < columns_; ++column) {
            if (!walkable(column, row)) {
                continue;
            }
            const SDL_FPoint position{std::min(column * cellSize + cellSize * 0.5F, width_ - 1.0F),
                                      std::clamp(row * cellSize + cellSize * 0.5F,
                                                 walkableTopY_,
                                                 height_ - 1.0F)};
            const float dx = position.x - desired.x;
            const float dy = position.y - desired.y;
            candidates_.push_back({dx * dx + dy * dy, position});
        }
    }
    std::sort(candidates_.begin(), candidates_.end(), [](const Candidate& a, const Candidate& b) {
        return a.distanceSquared < b.distanceSquared;
    });
    for (const Candidate& candidate : candidates_) {
        if (const auto route = findPath(from, candidate.position); !route.empty()) {
            return route;
        }
    }
    return {};
}
