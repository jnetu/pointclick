#include "graphics/FontLibrary.hpp"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace {

[[nodiscard]] std::size_t index(const FontRole role) {
    return static_cast<std::size_t>(role);
}

[[nodiscard]] std::string pathToUtf8(const std::filesystem::path& path) {
    const std::u8string text = path.u8string();
    return {text.begin(), text.end()};
}

constexpr FontRole kRoles[]{FontRole::dialogue, FontRole::choice, FontRole::speaker,
                            FontRole::title};
static_assert(std::size(kRoles) == kFontRoleCount);

} // namespace

FontLibrary::FontSpec FontLibrary::spec(const FontRole role) {
    switch (role) {
    case FontRole::dialogue:
        return {"atkinson-hyperlegible/AtkinsonHyperlegible-Regular.ttf", 30.0F, 2.0F};
    case FontRole::choice:
        return {"atkinson-hyperlegible/AtkinsonHyperlegible-Regular.ttf", 24.0F, 0.0F};
    case FontRole::speaker:
        return {"lora/Lora-Regular.ttf", 26.0F, 2.0F};
    case FontRole::title:
        return {"lora/Lora-Regular.ttf", 44.0F, 2.0F};
    }
    return {"atkinson-hyperlegible/AtkinsonHyperlegible-Regular.ttf", 24.0F, 0.0F};
}

std::array<std::string_view, kFontRoleCount> FontLibrary::files() {
    std::array<std::string_view, kFontRoleCount> list{};
    for (const FontRole role : kRoles) list[index(role)] = spec(role).file;
    return list;
}

FontLibrary::~FontLibrary() {
    close();
}

bool FontLibrary::open(std::filesystem::path root, const float pixelScale) {
    close();
    if (!ttfInitialized_) {
        if (!TTF_Init()) {
            SDL_Log("SDL_ttf unavailable, game text uses the debug font: %s", SDL_GetError());
            return false;
        }
        ttfInitialized_ = true;
    }
    pixelScale_ = pixelScale;
    bool all = true;
    for (const FontRole role : kRoles) {
        const FontSpec font = spec(role);
        const std::string path = pathToUtf8(root / font.file);
        SDL_IOStream* io = SDL_IOFromFile(path.c_str(), "rb");
        Loaded& loaded = fonts_[index(role)];
        loaded.fill = io ? TTF_OpenFontIO(io, true, font.pointSize * pixelScale_) : nullptr;
        if (!loaded.fill) {
            SDL_Log("Font unavailable, its text uses the debug font: %s (%s)", path.c_str(),
                    SDL_GetError());
            all = false;
            continue;
        }
        if (font.outline > 0.0F) {
            loaded.outline = TTF_CopyFont(loaded.fill);
            if (!loaded.outline) SDL_Log("Font outline unavailable: %s", SDL_GetError());
        }
        applySize(role);
    }
    return all;
}

void FontLibrary::applySize(const FontRole role) {
    Loaded& loaded = fonts_[index(role)];
    const FontSpec font = spec(role);
    if (loaded.fill) {
        (void)TTF_SetFontSize(loaded.fill, font.pointSize * pixelScale_);
        TTF_SetFontHinting(loaded.fill, TTF_HINTING_LIGHT);
    }
    if (loaded.outline) {
        (void)TTF_SetFontSize(loaded.outline, font.pointSize * pixelScale_);
        TTF_SetFontHinting(loaded.outline, TTF_HINTING_LIGHT);
        (void)TTF_SetFontOutline(loaded.outline, outlinePixels(role));
    }
}

void FontLibrary::setPixelScale(const float pixelScale) {
    if (pixelScale == pixelScale_) return;
    pixelScale_ = pixelScale;
    for (const FontRole role : kRoles) applySize(role);
}

void FontLibrary::close() {
    for (Loaded& loaded : fonts_) {
        if (loaded.outline) TTF_CloseFont(loaded.outline);
        if (loaded.fill) TTF_CloseFont(loaded.fill);
        loaded = {};
    }
    if (ttfInitialized_) {
        TTF_Quit();
        ttfInitialized_ = false;
    }
}

TTF_Font* FontLibrary::fill(const FontRole role) const {
    return fonts_[index(role)].fill;
}

TTF_Font* FontLibrary::outline(const FontRole role) const {
    return fonts_[index(role)].outline;
}

int FontLibrary::outlinePixels(const FontRole role) const {
    const float outline = spec(role).outline;
    return outline > 0.0F ? std::max(1, static_cast<int>(std::lround(outline * pixelScale_))) : 0;
}
