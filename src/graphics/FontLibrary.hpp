#pragma once

#include "ui/TextMetrics.hpp"

#include <array>
#include <filesystem>
#include <string_view>

typedef struct TTF_Font TTF_Font;

// The only place that knows which file and size draw each FontRole:
//   dialogue, choice -> assets/fonts/atkinson-hyperlegible (text read on screen)
//   speaker, title   -> assets/fonts/lora (names and headings)
// Sizes are in logical viewport pixels; fonts open at size * pixelScale so glyphs stay crisp on
// high-density screens. Fonts are read through SDL_IOStream, so the same path works from the APK
// or app bundle. A font that fails to open is logged once and left null (callers fall back).
class FontLibrary {
public:
    struct FontSpec {
        std::string_view file; // relative to the fonts directory
        float pointSize;       // logical pixels
        float outline;         // logical pixels of dark outline; 0 = none
    };
    [[nodiscard]] static FontSpec spec(FontRole role);

    FontLibrary() = default;
    ~FontLibrary();
    FontLibrary(const FontLibrary&) = delete;
    FontLibrary& operator=(const FontLibrary&) = delete;

    // Opens every role from root (TTF_Init on the first call). False when any role is missing.
    bool open(std::filesystem::path root, float pixelScale);
    // Re-sizes the open fonts for a new pixel scale (window moved to another display, resize).
    void setPixelScale(float pixelScale);
    void close();

    [[nodiscard]] float pixelScale() const { return pixelScale_; }
    // Null when that role failed to load.
    [[nodiscard]] TTF_Font* fill(FontRole role) const;
    [[nodiscard]] TTF_Font* outline(FontRole role) const;
    // Outline thickness in output pixels at the current scale.
    [[nodiscard]] int outlinePixels(FontRole role) const;

private:
    struct Loaded {
        TTF_Font* fill = nullptr;
        TTF_Font* outline = nullptr;
    };

    void applySize(FontRole role);

    std::array<Loaded, kFontRoleCount> fonts_{};
    float pixelScale_ = 1.0F;
    bool ttfInitialized_ = false;
};
