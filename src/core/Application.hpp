#pragma once

#include "debug/DebugEditor.hpp"
#include "game/Game.hpp"
#include "graphics/Renderer.hpp"
#include "input/InputRouter.hpp"
#include "input/InputSystem.hpp"

struct SDL_Window;

class Application {
public:
    explicit Application(std::filesystem::path roomDirectory = {});
    int run();

private:
    bool initialize();
    void mainLoop();
    void processEvents();
    void syncTextInputState();
    void shutdown();

    SDL_Window* window_ = nullptr;
    Game game_;
    Renderer renderer_;
    DebugEditor debugEditor_;
    InputSystem inputSystem_;
    InputRouter inputRouter_;
    float renderInterpolation_ = 1.0F;
    bool running_ = false;
    bool suspended_ = false;
    bool resetFrameClock_ = false;
};
