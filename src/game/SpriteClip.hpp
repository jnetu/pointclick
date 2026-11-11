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
    bool flipX = false;          // mirror horizontally (reuse a `right` sheet as `left`)
    float displayWidth = 0.0F;   // 0 = use object's/player's base width
    float displayHeight = 0.0F;  // 0 = use object's/player's base height
    float offsetX = 0.0F;        // base units, relative to feet/bottom center
    float offsetY = 0.0F;

    [[nodiscard]] bool operator==(const SpriteClip&) const = default;
    [[nodiscard]] std::string validationError() const;
    [[nodiscard]] int frameAt(float elapsedSeconds) const;
    [[nodiscard]] SDL_FRect targetRect(SDL_FPoint feet, float baseWidth,
                                        float baseHeight, float scale = 1.0F) const;
};

// Pure sheet arithmetic shared by the renderer and the content check (no PNG is opened here).
// Columns of a sheet imageWidth wide: the clip's own `columns`, or as many frames as fit.
[[nodiscard]] int spriteSheetColumns(const SpriteClip& clip, int imageWidth);
// True when every frame the clip uses lies inside an imageWidth x imageHeight PNG. A clip
// without frame dimensions is the whole image and always fits.
[[nodiscard]] bool clipFitsImage(const SpriteClip& clip, int imageWidth, int imageHeight);
