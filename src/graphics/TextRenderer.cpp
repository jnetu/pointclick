#include "graphics/TextRenderer.hpp"

#include "game/World.hpp"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <algorithm>
#include <cmath>

namespace {
constexpr float kDebugGlyph = 8.0F; // SDL debug font cell
constexpr std::uint64_t kEvictAfterFrames = 600;
constexpr std::uint64_t kEvictEveryFrames = 120;
constexpr SDL_Color kOutlineColor{10, 12, 18, 255};
} // namespace

TextRenderer::~TextRenderer() {
    shutdown();
}

void TextRenderer::initialize(SDL_Renderer* renderer) {
    renderer_ = renderer;
}

bool TextRenderer::setFontRoot(std::filesystem::path root) {
    root_ = std::move(root);
    clearCache();
    if (engine_) { // created for the fonts being closed: rebuilt below
        TTF_DestroyRendererTextEngine(engine_);
        engine_ = nullptr;
    }
    fontsOpen_ = fonts_.open(root_, targetPixelScale());
    createEngine();
    ++revision_;
    return fontsOpen_ && engine_ != nullptr;
}

void TextRenderer::createEngine() {
    if (engine_ || !renderer_ || !fonts_.fill(FontRole::dialogue)) return;
    engine_ = TTF_CreateRendererTextEngine(renderer_);
    if (!engine_) SDL_Log("SDL_ttf text engine unavailable, using the debug font: %s", SDL_GetError());
}

float TextRenderer::targetPixelScale() const {
    int width = 0;
    int height = 0;
    if (!renderer_ || !SDL_GetCurrentRenderOutputSize(renderer_, &width, &height) || width <= 0
        || height <= 0) {
        return 1.0F;
    }
    const float scale = std::min(static_cast<float>(width) / static_cast<float>(World::width),
                                 static_cast<float>(height) / static_cast<float>(World::height));
    // Quarter steps: a window being dragged larger does not re-raster fonts every frame.
    return std::clamp(std::round(scale * 4.0F) / 4.0F, 0.5F, 4.0F);
}

void TextRenderer::beginFrame() {
    ++frame_;
    if (fontsOpen_ || fonts_.fill(FontRole::dialogue)) {
        const float scale = targetPixelScale();
        if (scale != fonts_.pixelScale()) {
            clearCache();
            fonts_.setPixelScale(scale);
            ++revision_;
        }
    }
    if (frame_ % kEvictEveryFrames != 0) return;
    for (auto it = cache_.begin(); it != cache_.end();) {
        if (it->second.lastUsedFrame + kEvictAfterFrames < frame_) {
            if (it->second.outline) TTF_DestroyText(it->second.outline);
            if (it->second.fill) TTF_DestroyText(it->second.fill);
            it = cache_.erase(it);
        } else {
            ++it;
        }
    }
}

void TextRenderer::clearCache() {
    for (auto& [key, entry] : cache_) {
        if (entry.outline) TTF_DestroyText(entry.outline);
        if (entry.fill) TTF_DestroyText(entry.fill);
    }
    cache_.clear();
}

void TextRenderer::invalidate() {
    clearCache();
    if (engine_) {
        TTF_DestroyRendererTextEngine(engine_);
        engine_ = nullptr;
    }
    createEngine();
    ++revision_;
}

void TextRenderer::shutdown() {
    clearCache();
    if (engine_) {
        TTF_DestroyRendererTextEngine(engine_);
        engine_ = nullptr;
    }
    fonts_.close();
    fontsOpen_ = false;
    renderer_ = nullptr;
}

bool TextRenderer::hasFont(const FontRole role) const {
    return engine_ != nullptr && fonts_.fill(role) != nullptr;
}

float TextRenderer::fallbackScale(const FontRole role) {
    return std::max(1.0F, std::round(FontLibrary::spec(role).pointSize / 14.0F));
}

float TextRenderer::lineHeight(const FontRole role) const {
    if (!hasFont(role)) return kDebugGlyph * fallbackScale(role) * 1.25F;
    return static_cast<float>(TTF_GetFontLineSkip(fonts_.fill(role))) / fonts_.pixelScale();
}

float TextRenderer::width(const FontRole role, const std::string_view text) const {
    if (text.empty()) return 0.0F;
    if (!hasFont(role)) {
        return MonospaceTextMetrics(kDebugGlyph * fallbackScale(role), 0.0F).width(role, text);
    }
    int w = 0;
    int h = 0;
    if (!TTF_GetStringSize(fonts_.fill(role), text.data(), text.size(), &w, &h)) return 0.0F;
    return static_cast<float>(w) / fonts_.pixelScale();
}

void TextRenderer::draw(const FontRole role, const std::string_view text, const SDL_FPoint origin,
                        const SDL_Color color) {
    if (text.empty() || !renderer_) return;
    if (!hasFont(role)) {
        drawFallback(role, text, origin, color);
        return;
    }
    key_.assign(1, static_cast<char>('0' + static_cast<int>(role)));
    key_.append(text);
    auto found = cache_.find(key_);
    if (found == cache_.end()) {
        Entry entry;
        entry.fill = TTF_CreateText(engine_, fonts_.fill(role), text.data(), text.size());
        if (TTF_Font* outlineFont = fonts_.outline(role)) {
            entry.outline = TTF_CreateText(engine_, outlineFont, text.data(), text.size());
        }
        if (!entry.fill) {
            if (entry.outline) TTF_DestroyText(entry.outline);
            drawFallback(role, text, origin, color);
            return;
        }
        found = cache_.emplace(key_, entry).first;
    }
    Entry& entry = found->second;
    entry.lastUsedFrame = frame_;

    // Draw in output pixels: the font was rasterized at this scale, so one glyph texel lands on
    // one screen pixel and the position snaps to the pixel grid.
    const float scale = fonts_.pixelScale();
    float scaleX = 1.0F;
    float scaleY = 1.0F;
    SDL_GetRenderScale(renderer_, &scaleX, &scaleY);
    SDL_SetRenderScale(renderer_, scaleX / scale, scaleY / scale);
    const float x = std::round(origin.x * scale);
    const float y = std::round(origin.y * scale);
    if (entry.outline) {
        // An outlined font's text lays out `thickness` further right/down and each glyph grows by
        // `thickness` on every side: back off twice to center the outline on the fill.
        const auto thickness = static_cast<float>(fonts_.outlinePixels(role));
        (void)TTF_SetTextColor(entry.outline, kOutlineColor.r, kOutlineColor.g, kOutlineColor.b,
                               color.a);
        (void)TTF_DrawRendererText(entry.outline, x - 2.0F * thickness, y - 2.0F * thickness);
    }
    (void)TTF_SetTextColor(entry.fill, color.r, color.g, color.b, color.a);
    (void)TTF_DrawRendererText(entry.fill, x, y);
    SDL_SetRenderScale(renderer_, scaleX, scaleY);
}

void TextRenderer::drawFallback(const FontRole role, const std::string_view text,
                                const SDL_FPoint origin, const SDL_Color color) {
    key_.assign(text); // SDL_RenderDebugText needs a NUL-terminated string
    const float scale = fallbackScale(role);
    float scaleX = 1.0F;
    float scaleY = 1.0F;
    SDL_GetRenderScale(renderer_, &scaleX, &scaleY);
    SDL_SetRenderScale(renderer_, scaleX * scale, scaleY * scale);
    const float x = origin.x / scale;
    const float y = (origin.y + kDebugGlyph * scale * 0.125F) / scale;
    if (FontLibrary::spec(role).outline > 0.0F) {
        SDL_SetRenderDrawColor(renderer_, kOutlineColor.r, kOutlineColor.g, kOutlineColor.b,
                               color.a);
        SDL_RenderDebugText(renderer_, x + 1.0F / scale * 2.0F, y + 1.0F / scale * 2.0F,
                            key_.c_str());
    }
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    SDL_RenderDebugText(renderer_, x, y, key_.c_str());
    SDL_SetRenderScale(renderer_, scaleX, scaleY);
}
