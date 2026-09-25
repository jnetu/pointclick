#pragma once

#include "game/SceneObject.hpp"

#include <SDL3/SDL_rect.h>

#include <array>
#include <vector>

// All positions use the 1280 x 720 logical canvas. A new room can replace
// these values and object placements without changing movement or rendering.
struct Room {
    float wallBottomY = 280.0F;
    float walkableTopY = 300.0F;
    float farDepthY = 300.0F;
    float nearDepthY = 720.0F;
    float farScale = 0.55F;
    float nearScale = 1.65F;

    SDL_Color wallColor{171, 62, 67, 255};
    SDL_Color floorColor{42, 102, 175, 255};
    SDL_Color wallTrimColor{109, 39, 48, 255};
    SDL_Color floorGuideColor{68, 124, 190, 255};
    SDL_Color playerColor{245, 194, 102, 255};
    SDL_Color playerOutlineColor{255, 239, 205, 255};
    float vanishingPointX = 640.0F;
    int floorRaySpacing = 160;
    std::array<float, 4> floorGuideFractions{0.18F, 0.38F, 0.62F, 0.85F};
    SDL_FPoint playerStart{640.0F, 360.0F};
    SDL_FPoint playerBaseSize{48.0F, 80.0F};
    float playerFarSpeed = 110.0F;
    float playerNearSpeed = 330.0F;
    float collisionPaddingX = 40.0F;
    float collisionPaddingY = 8.0F;
    std::vector<SceneObject> scenery;

    [[nodiscard]] float scaleAt(float feetY) const;
    [[nodiscard]] float speedAt(float feetY) const;
    [[nodiscard]] SDL_FRect playerBounds(SDL_FPoint feet) const;
    [[nodiscard]] bool containsFloor(SDL_FPoint point) const;
    [[nodiscard]] SDL_FPoint clampDestination(SDL_FPoint point) const;

    [[nodiscard]] static Room firstRoom();

private:
    [[nodiscard]] float depthAt(float feetY) const;
};
