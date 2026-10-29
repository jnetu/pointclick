#pragma once

#include <SDL3/SDL_rect.h>
#include "graphics/SpriteLibrary.hpp"
#include "input/InputSystem.hpp"

struct SDL_Renderer;
struct SDL_Window;
class Game;
class DebugEditor;

class Renderer {
public:
    bool initialize(SDL_Window* window);
    void setSpriteRoot(std::filesystem::path root);
    void invalidateTextures();
    [[nodiscard]] bool windowToViewport(float x, float y, SDL_FPoint& result) const;
    [[nodiscard]] bool touchToViewport(float normalizedX, float normalizedY,
                                       SDL_FPoint& result) const;
    [[nodiscard]] bool windowToWorld(float x, float y, SDL_FPoint& result,
                                     SDL_FPoint cameraOffset = {}) const;
    [[nodiscard]] ViewportCoordinateMapper viewportMapper() const noexcept;
    [[nodiscard]] static bool containsViewportPoint(SDL_FPoint viewportPoint);
    void render(const Game& game, float interpolation, const DebugEditor* editor = nullptr,
                const InputSystem* input = nullptr);
    void shutdown();

private:
    SDL_Renderer* renderer_ = nullptr;
    SDL_Window* window_ = nullptr;
    SpriteLibrary sprites_;
    unsigned long long lastRoomRevision_ = 0;
};
