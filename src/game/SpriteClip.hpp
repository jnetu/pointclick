#pragma once

#include <SDL3/SDL_rect.h>
#include <string>

// Asset and frame layout only. The renderer owns PNG decoding and GPU textures.
// Zero frame dimensions mean a single static image using the entire PNG.
struct SpriteClip {
    std::string file;
    int frameWidth = 0;
    int frameHeight = 0;
    int frames = 1;
    int columns = 0;       // 0 = derive from PNG width
    int firstFrame = 0;    // row-major offset in the sheet
    int marginX = 0;
    int marginY = 0;
    int spacingX = 0;
    int spacingY = 0;
    float fps = 8.0F;
    bool loop = true;
    bool pixelated = true;
    float displayWidth = 0.0F;   // 0 = use object's/player's base width
    float displayHeight = 0.0F;  // 0 = use object's/player's base height
    float offsetX = 0.0F;        // base units, relative to feet/bottom center
    float offsetY = 0.0F;

    [[nodiscard]] std::string validationError() const;
    [[nodiscard]] int frameAt(float elapsedSeconds) const;
    [[nodiscard]] SDL_FRect targetRect(SDL_FPoint feet, float baseWidth,
                                        float baseHeight, float scale = 1.0F) const;
};
