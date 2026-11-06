#pragma once

#include <SDL3/SDL_rect.h>
#include "graphics/SpriteLibrary.hpp"
#include "input/InputSystem.hpp"

#include <string>
#include <vector>

struct SDL_Renderer;
struct SDL_Window;
class Game;
class DialogueMenu;
class RenderOverlay;

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
    // Overlays draw in registration order after the scene and dialogue menu; not owned.
    void addOverlay(const RenderOverlay* overlay);
    void render(const Game& game, float interpolation, const InputSystem* input = nullptr,
                const DialogueMenu* dialogueMenu = nullptr);
    void shutdown();

private:
    SDL_Renderer* renderer_ = nullptr;
    SDL_Window* window_ = nullptr;
    SpriteLibrary sprites_;
    std::string lastRoomId_;
    std::vector<const RenderOverlay*> overlays_;
};
