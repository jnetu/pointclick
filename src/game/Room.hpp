#pragma once

#include "game/SceneObject.hpp"

#include <SDL3/SDL_rect.h>

#include <array>
#include <string>
#include <vector>

// Validated room definition. Designers author assets/rooms/*.room; field defaults
// are used for omitted settings. Pixel values always use the logical canvas.
struct Room {
    std::string id = "first";
    // Room geometry and the shared depth range for size and movement speed.
    float wallBottomY = 280.0F;
    float walkableTopY = 300.0F;
    float farDepthY = 300.0F;
    float nearDepthY = 720.0F;
    float farScale = 0.55F;
    float nearScale = 1.65F;

    // Placeholder art and perspective guides (replace with sprites later).
    SDL_Color wallColor{171, 62, 67, 255};
    SDL_Color floorColor{42, 102, 175, 255};
    SDL_Color wallTrimColor{109, 39, 48, 255};
    SDL_Color floorGuideColor{68, 124, 190, 255};
    SDL_Color playerColor{245, 194, 102, 255};
    SDL_Color playerOutlineColor{255, 239, 205, 255};
    float vanishingPointX = 640.0F;
    int floorRaySpacing = 160;
    std::array<float, 4> floorGuideFractions{0.18F, 0.38F, 0.62F, 0.85F};
    // Player tuning for this room; the live position belongs to Player.
    SDL_FPoint playerStart{640.0F, 360.0F};
    SDL_FPoint playerBaseSize{48.0F, 80.0F};
    float playerFarSpeed = 110.0F;
    float playerNearSpeed = 330.0F;
    // Extra space around solid footprints before rasterizing the navigation grid.
    float collisionPaddingX = 40.0F;
    float collisionPaddingY = 8.0F;
    std::vector<SceneObject> scenery;

    [[nodiscard]] float scaleAt(float feetY) const;
    [[nodiscard]] float speedAt(float feetY) const;
    [[nodiscard]] SDL_FRect playerBounds(SDL_FPoint feet) const;
    [[nodiscard]] bool containsFloor(SDL_FPoint point) const;
    [[nodiscard]] SDL_FPoint clampDestination(SDL_FPoint point) const;

    [[nodiscard]] static Room firstRoom();
    // Empty means valid. Invalid designer data must never enter the simulation.
    [[nodiscard]] std::string validationError() const;

private:
    [[nodiscard]] float depthAt(float feetY) const;
};
