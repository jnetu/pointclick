#pragma once

#include "input/InputSystem.hpp"

#include <span>
#include <string_view>
#include <vector>

class Game;
class DebugEditor;

// Extension point for future gameplay UIs (Dialogue, Inventory, Pause Menu)
// operating in the fixed 1280x720 logical viewport before world conversion.
class ViewportUiConsumer {
public:
    virtual ~ViewportUiConsumer() = default;
    virtual bool onPointerMove([[maybe_unused]] SDL_FPoint viewportPoint,
                               [[maybe_unused]] bool insideViewport) { return false; }
    virtual bool onPrimaryPress([[maybe_unused]] SDL_FPoint viewportPoint) { return false; }
    virtual bool onNavigation([[maybe_unused]] UiNavAction action) { return false; }
    virtual bool onTextInput([[maybe_unused]] std::string_view text) { return false; }
};

struct InputRouteResult {
    bool quitRequested = false;
    bool enteredBackground = false;
    bool enteredForeground = false;
    bool invalidateGraphics = false;
    bool consumedByDebug = false;
    bool consumedByUi = false;
};

// Priority Consumer Chain:
//   1. Lifecycle & System (quit, background/foreground, render device/targets reset)
//   2. Tools (DebugEditor keys, text, and viewport panel hit-test)
//   3. Gameplay UI (ViewportUiConsumer instances in 1280x720 viewport space)
//   4. World (Camera::viewportToWorld -> Game::onPointerMove / Game::onClick)
class InputRouter {
public:
    void setUiConsumers(std::span<ViewportUiConsumer* const> consumers);
    void addUiConsumer(ViewportUiConsumer* consumer);
    void clearUiConsumers() noexcept;

    [[nodiscard]] InputRouteResult routeTranslated(const TranslatedInput& translated,
                                                   DebugEditor& debugEditor,
                                                   Game& game,
                                                   float interpolation = 1.0F) const;

    [[nodiscard]] InputRouteResult handleEvent(
            const SDL_Event& event,
            InputSystem& input,
            DebugEditor& debugEditor,
            Game& game,
            const ViewportCoordinateMapper& mapper = ViewportCoordinateMapper::identity(),
            float interpolation = 1.0F) const;

    void update(float deltaSeconds,
                InputSystem& input,
                DebugEditor& debugEditor,
                Game& game,
                float interpolation = 1.0F) const;

private:
    std::vector<ViewportUiConsumer*> uiConsumers_;
};
