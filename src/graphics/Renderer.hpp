#pragma once

#include <SDL3/SDL_rect.h>
#include "graphics/SpriteLibrary.hpp"

struct SDL_Renderer;
struct SDL_Window;
class Game;
class DebugEditor;

class Renderer {
public:
    bool initialize(SDL_Window* window);
    void setSpriteRoot(std::filesystem::path root);
    [[nodiscard]] bool windowToWorld(float x, float y, SDL_FPoint& result) const;
    void render(const Game& game, float interpolation, const DebugEditor* editor = nullptr);
    void shutdown();

private:
    SDL_Renderer* renderer_ = nullptr;
    SDL_Window* window_ = nullptr;
    SpriteLibrary sprites_;
    unsigned long long lastRoomRevision_ = 0;
};
