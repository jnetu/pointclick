#pragma once

struct SDL_Renderer;
struct SDL_Window;
class Game;

class Renderer {
public:
    bool initialize(SDL_Window* window);
    void render(const Game& game, float interpolation);
    void shutdown();

private:
    SDL_Renderer* renderer_ = nullptr;
};
