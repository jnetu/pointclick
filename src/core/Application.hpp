#pragma once

#include "game/Game.hpp"
#include "graphics/Renderer.hpp"

struct SDL_Window;

class Application {
public:
    int run();

private:
    bool initialize();
    void mainLoop();
    void processEvents();
    void shutdown();

    SDL_Window* window_ = nullptr;
    Game game_;
    Renderer renderer_;
    bool running_ = false;
};
