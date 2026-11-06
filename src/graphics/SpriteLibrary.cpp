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

namespace {
std::string pathToUtf8(const std::filesystem::path& path) {
    const auto utf8 = path.u8string();
    return {reinterpret_cast<const char*>(utf8.data()), utf8.size()};
}

// The sheet slots a clip uses are row-major from firstFrame; checking the furthest column and
// row they reach is equivalent to checking every frame, in O(1).
bool clipFitsImage(const SpriteClip& clip, const int columns, const int imageWidth,
                   const int imageHeight) {
    const int strideX = clip.frameWidth + clip.spacingX;
    const int strideY = clip.frameHeight + clip.spacingY;
    const int firstSlot = clip.firstFrame;
    const int lastSlot = clip.firstFrame + clip.frames - 1;
    const int lastRow = lastSlot / columns;
    const int furthestColumn = firstSlot / columns == lastRow
                               ? lastSlot % columns
                               : columns - 1;
    return clip.marginX + furthestColumn * strideX + clip.frameWidth <= imageWidth
           && clip.marginY + lastRow * strideY + clip.frameHeight <= imageHeight;
}
} // namespace

void SpriteLibrary::beginRoom() {
    ++roomGeneration_;
    for (auto it = images_.begin(); it != images_.end();) {
        // Keep what the current and the previous room used: A -> B -> A reloads nothing.
        if (it->second.lastUsedRoom + 2 < roomGeneration_) {
            if (it->second.texture) SDL_DestroyTexture(it->second.texture);
            it = images_.erase(it);
        } else {
            ++it;
        }
    }
}

SpriteLibrary::Image& SpriteLibrary::imageFor(const std::string& file) {
    if (const auto found = images_.find(file); found != images_.end()) {
        found->second.lastUsedRoom = roomGeneration_;
        return found->second;
    }
    Image image;
    const std::string path = pathToUtf8(root_ / file);
    SDL_Surface* surface = SDL_LoadPNG(path.c_str());
    image.lastUsedRoom = roomGeneration_;
    if (surface) {
        image.width = surface->w;
        image.height = surface->h;
        image.texture = SDL_CreateTextureFromSurface(renderer_, surface);
        SDL_DestroySurface(surface);
        if (image.texture) SDL_SetTextureBlendMode(image.texture, SDL_BLENDMODE_BLEND);
    }
    if (!image.texture && missingLogged_.insert(file).second) {
        SDL_Log("Sprite PNG unavailable: %s (%s)", path.c_str(), SDL_GetError());
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
    // Clips were validated when the room was applied (Room::validationError); drawing only
    // checks what depends on the loaded PNG.
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
        if (!clipFitsImage(clip, columns, image.width, image.height)) {
            drawMissing(target);
            return false;
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
