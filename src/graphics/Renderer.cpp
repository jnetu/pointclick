#include "graphics/Renderer.hpp"
#include "graphics/SceneRenderer.hpp"
#include "graphics/DiagnosticsOverlay.hpp"
#include "graphics/DebugOverlay.hpp"
#include "game/World.hpp"
#include "game/Game.hpp"
#include <SDL3/SDL.h>

bool Renderer::initialize(SDL_Window* window) {
    window_ = window;
    renderer_ = SDL_CreateRenderer(window, nullptr);
    if (renderer_ == nullptr) {
        SDL_Log("Could not create the renderer: %s", SDL_GetError());
        return false;
    }
    if (!SDL_SetRenderLogicalPresentation(renderer_, World::width, World::height,
                                          SDL_LOGICAL_PRESENTATION_LETTERBOX)) {
        SDL_Log("Could not set logical presentation: %s", SDL_GetError());
        return false;
    }
    sprites_.initialize(renderer_);
    return true;
}

void Renderer::setSpriteRoot(std::filesystem::path root) {
    sprites_.setRoot(std::move(root));
}

void Renderer::invalidateTextures() {
    sprites_.clear();
}

bool Renderer::windowToViewport(const float x, const float y, SDL_FPoint& result) const {
    return SDL_RenderCoordinatesFromWindow(renderer_, x, y, &result.x, &result.y);
}

bool Renderer::touchToViewport(const float normalizedX, const float normalizedY,
                               SDL_FPoint& result) const {
    if (window_ == nullptr) {
        return false;
    }
    int windowWidth = 0;
    int windowHeight = 0;
    if (!SDL_GetWindowSize(window_, &windowWidth, &windowHeight)
        || windowWidth <= 0 || windowHeight <= 0) {
        return false;
    }
    return windowToViewport(normalizedX * static_cast<float>(windowWidth),
                            normalizedY * static_cast<float>(windowHeight), result);
}

bool Renderer::windowToWorld(const float x, const float y, SDL_FPoint& result,
                             const SDL_FPoint cameraOffset) const {
    if (!windowToViewport(x, y, result)) {
        return false;
    }
    result.x += cameraOffset.x;
    result.y += cameraOffset.y;
    return true;
}

ViewportCoordinateMapper Renderer::viewportMapper() const noexcept {
    return {
        .context = this,
        .windowToViewport = [](const void* ctx, const float wx, const float wy,
                               SDL_FPoint& out) {
            return static_cast<const Renderer*>(ctx)->windowToViewport(wx, wy, out);
        },
        .touchToViewport = [](const void* ctx, const float nx, const float ny,
                              SDL_FPoint& out) {
            return static_cast<const Renderer*>(ctx)->touchToViewport(nx, ny, out);
        },
    };
}

bool Renderer::containsViewportPoint(const SDL_FPoint viewportPoint) {
    return World::containsViewportPoint(viewportPoint);
}

void Renderer::render(const Game& game, const float interpolation,
                      const DebugEditor* editor, const InputSystem* input) {
    SDL_SetRenderDrawColor(renderer_, 8, 10, 16, 255);
    SDL_RenderClear(renderer_);
    if (lastRoomRevision_ != game.roomRevision()) {
        sprites_.clear();
        lastRoomRevision_ = game.roomRevision();
    }
    SceneRenderer::draw(renderer_, sprites_, game, interpolation);
    DiagnosticsOverlay::draw(renderer_, window_, game, interpolation);
    if (editor) DebugOverlay::draw(renderer_, game, *editor, interpolation);
    if (input && input->activeDevice() == InputDevice::gamepad) {
        const SDL_FPoint cursor = input->cursorViewport();
        SDL_SetRenderDrawColor(renderer_, 12, 18, 28, 220);
        const SDL_FRect centerBox{cursor.x - 2.0F, cursor.y - 2.0F, 5.0F, 5.0F};
        SDL_RenderFillRect(renderer_, &centerBox);
        SDL_SetRenderDrawColor(renderer_, 255, 240, 168, 255);
        SDL_RenderLine(renderer_, cursor.x - 10.0F, cursor.y, cursor.x - 3.0F, cursor.y);
        SDL_RenderLine(renderer_, cursor.x + 3.0F, cursor.y, cursor.x + 10.0F, cursor.y);
        SDL_RenderLine(renderer_, cursor.x, cursor.y - 10.0F, cursor.x, cursor.y - 3.0F);
        SDL_RenderLine(renderer_, cursor.x, cursor.y + 3.0F, cursor.x, cursor.y + 10.0F);
    }
    SDL_RenderPresent(renderer_);
}

void Renderer::shutdown() {
    sprites_.clear();
    if (renderer_ != nullptr) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    window_ = nullptr;
}
