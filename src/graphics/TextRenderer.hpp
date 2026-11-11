#pragma once

#include "game/StringMap.hpp"
#include "graphics/FontLibrary.hpp"
#include "ui/TextMetrics.hpp"

#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_rect.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

struct SDL_Renderer;
typedef struct TTF_Text TTF_Text;
typedef struct TTF_TextEngine TTF_TextEngine;

// Game text with SDL3_ttf: UTF-8, measured and drawn in logical viewport units. Fonts open at the
// renderer's output scale (logical -> pixels, so DPI and window size), and each line is drawn 1:1
// in output pixels, so text stays crisp on any screen. Each (role, line) keeps a TTF_Text in a
// cache, so a line on screen for many frames is shaped once; the renderer text engine keeps the
// glyph atlas. Lines unused for a while are released.
//
// A role whose font did not load (or no SDL_ttf at all) falls back to the SDL debug font, with
// metrics to match, so the game still shows its text; the failure is logged once.
class TextRenderer final : public TextMetrics {
public:
    TextRenderer() = default;
    ~TextRenderer() override;
    TextRenderer(const TextRenderer&) = delete;
    TextRenderer& operator=(const TextRenderer&) = delete;

    void initialize(SDL_Renderer* renderer);
    // <assets>/fonts. Opens every role; false (and debug-font fallback) when one is missing.
    bool setFontRoot(std::filesystem::path root);
    // Once per frame, before drawing: follows the output scale and releases stale lines.
    void beginFrame();
    // Render device reset: glyph atlases are gone, rebuild them on the next draw.
    void invalidate();
    void shutdown();

    [[nodiscard]] bool hasFont(FontRole role) const;
    [[nodiscard]] float pixelScale() const { return fonts_.pixelScale(); }
    [[nodiscard]] std::size_t cachedLineCount() const { return cache_.size(); }

    [[nodiscard]] float lineHeight(FontRole role) const override;
    [[nodiscard]] float width(FontRole role, std::string_view text) const override;
    [[nodiscard]] std::uint32_t revision() const override { return revision_; }

    // Draws one line with its top-left at origin (viewport units). Outlined roles get a dark
    // outline (FontLibrary::spec) so they read over any background.
    void draw(FontRole role, std::string_view text, SDL_FPoint origin, SDL_Color color);

private:
    struct Entry {
        TTF_Text* fill = nullptr;
        TTF_Text* outline = nullptr;
        std::uint64_t lastUsedFrame = 0;
    };

    void createEngine();
    void clearCache();
    [[nodiscard]] float targetPixelScale() const;
    [[nodiscard]] static float fallbackScale(FontRole role);
    void drawFallback(FontRole role, std::string_view text, SDL_FPoint origin, SDL_Color color);

    SDL_Renderer* renderer_ = nullptr;
    TTF_TextEngine* engine_ = nullptr;
    FontLibrary fonts_;
    std::filesystem::path root_;
    bool fontsOpen_ = false;
    StringMap<Entry> cache_;
    std::string key_; // reused lookup key / NUL-terminated copy for the debug font
    std::uint64_t frame_ = 0;
    std::uint32_t revision_ = 1;
};
