#pragma once

#include <SDL3/SDL_rect.h>

struct SDL_Renderer;
struct SDL_Window;
class Game;

class Renderer {
public:
    bool initialize(SDL_Window* window);
    [[nodiscard]] bool windowToWorld(float x, float y, SDL_FPoint& result) const;
    void render(const Game& game, float interpolation);
    void shutdown();

private:
    SDL_Renderer* renderer_ = nullptr;
    SDL_Window* window_ = nullptr;
};
