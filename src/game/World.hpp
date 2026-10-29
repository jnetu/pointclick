#pragma once

#include <SDL3/SDL_rect.h>

#include <algorithm>

// Fixed logical camera viewport (field of view). Room bounds live in Room.
namespace World {
constexpr int width = 1280;
constexpr int height = 720;

[[nodiscard]] constexpr bool containsViewportPoint(const SDL_FPoint viewportPoint) {
    return viewportPoint.x >= 0.0F && viewportPoint.x < static_cast<float>(width)
           && viewportPoint.y >= 0.0F && viewportPoint.y < static_cast<float>(height);
}

[[nodiscard]] constexpr SDL_FPoint clampToViewport(const SDL_FPoint viewportPoint) {
    return {
        std::clamp(viewportPoint.x, 0.0F, static_cast<float>(width - 1)),
        std::clamp(viewportPoint.y, 0.0F, static_cast<float>(height - 1)),
    };
}
}
