#pragma once

#include "game/Game.hpp"
#include "graphics/Renderer.hpp"
#include "debug/DebugEditor.hpp"

struct SDL_Window;

class Application {
public:
    explicit Application(std::filesystem::path roomDirectory = {});
    int run();

private:
    bool initialize();
    void mainLoop();
    void processEvents();
    void shutdown();

    SDL_Window* window_ = nullptr;
    Game game_;
    Renderer renderer_;
    DebugEditor debugEditor_;
    bool running_ = false;
};
