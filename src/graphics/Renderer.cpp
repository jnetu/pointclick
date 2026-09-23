#include "graphics/Renderer.hpp"

#include "game/Game.hpp"

#include <SDL3/SDL.h>
constexpr float textScale = 16.0F;

bool Renderer::initialize(SDL_Window* window) {
    renderer_ = SDL_CreateRenderer(window, nullptr);
    if (renderer_ == nullptr) {
        SDL_Log("Could not create the renderer: %s", SDL_GetError());
        return false;
    }

    return true;
}

void Renderer::render(const Game& game, const float interpolation) {
    SDL_SetRenderDrawColor(renderer_, 18, 18, 24, 255);
    SDL_RenderClear(renderer_);

    SDL_SetRenderDrawColor(renderer_, 150, 160, 180, 255);
    SDL_RenderDebugText(renderer_, 20.0F, 20.0F, "Type a character");

    const FloatingText& text = game.floatingText();
    if (text.isVisible()) {
        SDL_SetRenderDrawColor(renderer_, 240, 220, 100, 255);
        SDL_SetRenderScale(renderer_, textScale, textScale);

        SDL_RenderDebugText(
                renderer_,
                text.interpolatedX(interpolation) / textScale,
                text.y() / textScale,
                text.text().c_str()
        );

        SDL_SetRenderScale(renderer_, 1.0F, 1.0F);
    }

    SDL_RenderPresent(renderer_);
}

void Renderer::shutdown() {
    if (renderer_ != nullptr) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
}
