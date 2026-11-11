#pragma once

#include <SDL3/SDL_rect.h>
#include "graphics/SpeechRenderer.hpp"
#include "graphics/SpriteLibrary.hpp"
#include "graphics/TextRenderer.hpp"
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
    // True when presenting waits for the display; otherwise the caller must pace the frames.
    [[nodiscard]] bool vsyncActive() const noexcept { return vsync_; }
    void setSpriteRoot(std::filesystem::path root);
    // <assets>/fonts; without it (or when a font fails) game text uses the debug font.
    bool setFontRoot(std::filesystem::path root);
    // Measures of the game text, for ui/ layouts (DialogueMenu). Lives as long as the Renderer.
    [[nodiscard]] const TextMetrics& textMetrics() const noexcept { return text_; }
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
    bool vsync_ = false;
    SpriteLibrary sprites_;
    TextRenderer text_;
    SpeechRenderer speech_;
    std::string lastRoomId_;
    std::vector<const RenderOverlay*> overlays_;
};
