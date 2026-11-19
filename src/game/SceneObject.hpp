#pragma once

#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_rect.h>
#include <optional>
#include <string>
#include "game/Animation.hpp"

enum class SceneObjectType { decoration, solid };

struct SceneObject {
    SDL_FRect bounds;
    SDL_Color color;
    SceneObjectType type = SceneObjectType::decoration;
    // Local to bounds. Only the object's base needs to block the player's feet.
    SDL_FRect collisionFootprint{};
    std::string id;
    // Empty means the colored placeholder. Gameplay selects an animation by name.
    AnimationSet animations;
    std::string initialAnimation = "idle";
    // Local to the top of bounds (D4): where the object meets the floor for depth. Unset = bounds.h.
    // The `{}` keeps Clang's -Wmissing-field-initializers quiet for aggregates that stop before it.
    std::optional<float> baseline{};

    [[nodiscard]] float depth() const { return bounds.y + baseline.value_or(bounds.h); }
    [[nodiscard]] SDL_FRect worldCollisionFootprint() const {
        return {bounds.x + collisionFootprint.x, bounds.y + collisionFootprint.y,
                collisionFootprint.w, collisionFootprint.h};
    }
};
