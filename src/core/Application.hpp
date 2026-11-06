#pragma once

#include "content/RoomLibrary.hpp"
#include "game/Game.hpp"
#include "graphics/Renderer.hpp"
#include "input/InputRouter.hpp"
#include "input/InputSystem.hpp"
#include "ui/DialogueMenu.hpp"

#if POINTCLICK_TOOLS
#include "debug/DebugEditor.hpp"
#include "debug/DebugOverlay.hpp"
#endif

#include <vector>

struct SDL_Window;

class Application {
public:
    explicit Application(std::filesystem::path roomDirectory = {});
    int run();

private:
    bool initialize();
    bool loadRoomWithRules(std::string_view roomFilename);
    // Routes the domain's notifications; today only room changes need the shell.
    void processGameEvents();
    void mainLoop();
    void processEvents();
    void syncTextInputState();
    void shutdown();

    SDL_Window* window_ = nullptr;
    std::filesystem::path roomDirectory_;
    RoomLibrary library_;
    Game game_;
    std::vector<GameEvent> gameEvents_; // reused drain buffer
    DialogueMenu dialogueMenu_{game_};
    Renderer renderer_;
#if POINTCLICK_TOOLS
    DebugEditor debugEditor_;
    DebugOverlay debugOverlay_{debugEditor_};
#endif
    InputSystem inputSystem_;
    InputRouter inputRouter_;
    float renderInterpolation_ = 1.0F;
    bool running_ = false;
    bool suspended_ = false;
    bool resetFrameClock_ = false;
};
