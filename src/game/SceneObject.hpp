#pragma once

#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_rect.h>

enum class SceneObjectType { decoration, solid };

struct SceneObject {
    SDL_FRect bounds;
    SDL_Color color;
    SceneObjectType type = SceneObjectType::decoration;
    // Local to bounds. Only the object's base needs to block the player's feet.
    SDL_FRect collisionFootprint{};

    [[nodiscard]] float depth() const { return bounds.y + bounds.h; }
    [[nodiscard]] SDL_FRect worldCollisionFootprint() const {
        return {bounds.x + collisionFootprint.x, bounds.y + collisionFootprint.y,
                collisionFootprint.w, collisionFootprint.h};
    }
};
