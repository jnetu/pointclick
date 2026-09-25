#pragma once

#include "game/SceneObject.hpp"
#include "game/SpriteClip.hpp"

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

    // Placeholder room art and perspective guides.
    SDL_Color wallColor{171, 62, 67, 255};
    SDL_Color floorColor{42, 102, 175, 255};
    SDL_Color wallTrimColor{109, 39, 48, 255};
    SDL_Color floorGuideColor{68, 124, 190, 255};
    SDL_Color playerColor{245, 194, 102, 255}; // Legacy room-file fields, unused with PNGs.
    SDL_Color playerOutlineColor{255, 239, 205, 255};
    float vanishingPointX = 640.0F;
    int floorRaySpacing = 160;
    std::array<float, 4> floorGuideFractions{0.18F, 0.38F, 0.62F, 0.85F};
    // Player tuning for this room; the live position belongs to Player.
    SDL_FPoint playerStart{640.0F, 360.0F};
    SDL_FPoint playerBaseSize{48.0F, 80.0F};
    float playerFarSpeed = 110.0F;
    float playerNearSpeed = 330.0F;
    PlayerSprites playerSprites{
        SpriteClip{.file = "player_idle.png", .frameWidth = 32, .frameHeight = 48,
                   .frames = 8, .columns = 1, .fps = 6, .displayWidth = 53.333333F},
        SpriteClip{.file = "player_esquerda.png", .frameWidth = 32, .frameHeight = 48,
                   .frames = 7, .columns = 7, .spacingX = 1, .displayWidth = 53.333333F},
        SpriteClip{.file = "player_direita.png", .frameWidth = 32, .frameHeight = 48,
                   .frames = 7, .columns = 7, .spacingX = 1, .displayWidth = 53.333333F},
        SpriteClip{.file = "player_cima.png", .frameWidth = 32, .frameHeight = 48,
                   .frames = 7, .columns = 7, .spacingX = 1, .displayWidth = 53.333333F},
        SpriteClip{.file = "player_baixo.png", .frameWidth = 32, .frameHeight = 48,
                   .frames = 7, .columns = 7, .spacingX = 1, .displayWidth = 53.333333F},
    };
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
