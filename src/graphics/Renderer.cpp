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

bool Renderer::windowToWorld(const float x, const float y, SDL_FPoint& result) const {
    return SDL_RenderCoordinatesFromWindow(renderer_, x, y, &result.x, &result.y);
}

void Renderer::render(const Game& game, const float interpolation, const DebugEditor* editor) {
    SDL_SetRenderDrawColor(renderer_, 8, 10, 16, 255);
    SDL_RenderClear(renderer_);
    if (lastRoomRevision_ != game.roomRevision()) {
        sprites_.clear();
        lastRoomRevision_ = game.roomRevision();
    }
    SceneRenderer::draw(renderer_, sprites_, game, interpolation);
    DiagnosticsOverlay::draw(renderer_, window_, game, interpolation);
    if (editor) DebugOverlay::draw(renderer_, game, *editor);
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
