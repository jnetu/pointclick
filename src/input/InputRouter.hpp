#pragma once

#include "input/InputSystem.hpp"

#include <span>
#include <string_view>
#include <vector>

class Game;

// Gameplay UI (dialogue menu, future inventory/pause) in the 1280x720 viewport, before the world.
class ViewportUiConsumer {
public:
    virtual ~ViewportUiConsumer() = default;
    virtual bool onPointerMove([[maybe_unused]] SDL_FPoint viewportPoint,
                               [[maybe_unused]] bool insideViewport) { return false; }
    virtual bool onPrimaryPress([[maybe_unused]] SDL_FPoint viewportPoint) { return false; }
    virtual bool onNavigation([[maybe_unused]] UiNavAction action) { return false; }
    virtual bool onTextInput([[maybe_unused]] std::string_view text) { return false; }
};

// Optional tools layer (in-game editor); the router never knows the concrete class.
class ToolInputConsumer {
public:
    virtual ~ToolInputConsumer() = default;
    virtual bool onToolAction(ToolAction action, Game& game) = 0;
    virtual bool onTextInput(std::string_view text, Game& game) = 0;
    [[nodiscard]] virtual bool consumesViewportPoint(SDL_FPoint viewportPoint) const = 0;
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
//   2. Tools (optional ToolInputConsumer: actions, text, and its viewport panel hit-test)
//   3. Gameplay UI (ViewportUiConsumer instances in 1280x720 viewport space)
//   4. World (Camera::viewportToWorld -> Game::onPointerMove / Game::onClick)
class InputRouter {
public:
    void setUiConsumers(std::span<ViewportUiConsumer* const> consumers);
    void addUiConsumer(ViewportUiConsumer* consumer);
    void clearUiConsumers() noexcept;
    // nullptr (the default) = no tools layer, as in store builds.
    void setToolConsumer(ToolInputConsumer* tools) noexcept { tools_ = tools; }

    [[nodiscard]] InputRouteResult routeTranslated(const TranslatedInput& translated,
                                                   Game& game,
                                                   float interpolation = 1.0F) const;

    [[nodiscard]] InputRouteResult handleEvent(
            const SDL_Event& event,
            InputSystem& input,
            Game& game,
            const ViewportCoordinateMapper& mapper = ViewportCoordinateMapper::identity(),
            float interpolation = 1.0F) const;

    void update(float deltaSeconds,
                InputSystem& input,
                Game& game,
                float interpolation = 1.0F) const;

private:
    std::vector<ViewportUiConsumer*> uiConsumers_;
    ToolInputConsumer* tools_ = nullptr;
};
