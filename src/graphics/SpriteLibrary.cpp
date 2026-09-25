#include "graphics/SpriteLibrary.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>

SpriteLibrary::~SpriteLibrary() { clear(); }

void SpriteLibrary::initialize(SDL_Renderer* renderer) {
    clear();
    renderer_ = renderer;
}

void SpriteLibrary::setRoot(std::filesystem::path root) {
    clear();
    missingLogged_.clear();
    root_ = std::move(root);
}

void SpriteLibrary::clear() {
    for (auto& [_, image] : images_) {
        if (image.texture) SDL_DestroyTexture(image.texture);
    }
    images_.clear();
}

SpriteLibrary::Image& SpriteLibrary::imageFor(const std::string& file) {
    if (const auto found = images_.find(file); found != images_.end()) return found->second;
    Image image;
    const auto path = root_ / file;
    SDL_Surface* surface = SDL_LoadPNG(path.string().c_str());
    if (surface) {
        image.width = surface->w;
        image.height = surface->h;
        image.texture = SDL_CreateTextureFromSurface(renderer_, surface);
        SDL_DestroySurface(surface);
        if (image.texture) SDL_SetTextureBlendMode(image.texture, SDL_BLENDMODE_BLEND);
    }
    if (!image.texture && missingLogged_.insert(file).second) {
        SDL_Log("Sprite PNG unavailable: %s (%s)", path.string().c_str(), SDL_GetError());
    }
    return images_.emplace(file, image).first->second;
}

void SpriteLibrary::drawMissing(const SDL_FRect target) const {
    if (!renderer_ || target.w <= 0 || target.h <= 0) return;
    constexpr float tile = 12.0F;
    for (int row = 0; row * tile < target.h; ++row) {
        for (int column = 0; column * tile < target.w; ++column) {
            if ((row + column) % 2 == 0) SDL_SetRenderDrawColor(renderer_, 145, 0, 190, 255);
            else SDL_SetRenderDrawColor(renderer_, 10, 7, 17, 255);
            const SDL_FRect part{target.x + column * tile, target.y + row * tile,
                                 std::min(tile, target.w - column * tile),
                                 std::min(tile, target.h - row * tile)};
            SDL_RenderFillRect(renderer_, &part);
        }
    }
}

bool SpriteLibrary::draw(const SpriteClip& clip, const SDL_FRect target,
                         const float elapsedSeconds, const bool flipHorizontally) {
    if (!renderer_ || target.w <= 0 || target.h <= 0) return false;
    if (!clip.validationError().empty()) { drawMissing(target); return false; }
    if (clip.file.empty()) { drawMissing(target); return false; }
    Image& image = imageFor(clip.file);
    if (!image.texture) { drawMissing(target); return false; }

    SDL_FRect source{0, 0, static_cast<float>(image.width), static_cast<float>(image.height)};
    if (clip.frameWidth > 0 && clip.frameHeight > 0) {
        const int strideX = clip.frameWidth + clip.spacingX;
        const int strideY = clip.frameHeight + clip.spacingY;
        const int columns = clip.columns > 0 ? clip.columns
                            : (image.width - clip.marginX + clip.spacingX) / strideX;
        if (columns <= 0 || strideY <= 0) { drawMissing(target); return false; }
        const auto frameRect = [&](const int index) -> SDL_FRect {
            const int slot = clip.firstFrame + index;
            return {static_cast<float>(clip.marginX + (slot % columns) * strideX),
                    static_cast<float>(clip.marginY + (slot / columns) * strideY),
                    static_cast<float>(clip.frameWidth), static_cast<float>(clip.frameHeight)};
        };
        for (int frame = 0; frame < clip.frames; ++frame) {
            const auto rectangle = frameRect(frame);
            if (rectangle.x < 0 || rectangle.y < 0
                || rectangle.x + rectangle.w > image.width
                || rectangle.y + rectangle.h > image.height) {
                drawMissing(target);
                return false;
            }
        }
        source = frameRect(clip.frameAt(elapsedSeconds));
    }
    SDL_SetTextureScaleMode(image.texture, clip.pixelated ? SDL_SCALEMODE_NEAREST
                                                  : SDL_SCALEMODE_LINEAR);
    if (!SDL_RenderTextureRotated(renderer_, image.texture, &source, &target, 0.0,
                                  nullptr, flipHorizontally ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE)) {
        drawMissing(target);
        return false;
    }
    return true;
}
