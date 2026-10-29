#include "input/InputRouter.hpp"
#include "debug/DebugEditor.hpp"
#include "game/Game.hpp"

void InputRouter::setUiConsumers(const std::span<ViewportUiConsumer* const> consumers) {
    uiConsumers_.assign(consumers.begin(), consumers.end());
}

void InputRouter::addUiConsumer(ViewportUiConsumer* consumer) {
    if (consumer != nullptr) {
        uiConsumers_.push_back(consumer);
    }
}

void InputRouter::clearUiConsumers() noexcept {
    uiConsumers_.clear();
}

InputRouteResult InputRouter::routeTranslated(const TranslatedInput& translated,
                                              DebugEditor& debugEditor,
                                              Game& game,
                                              const float interpolation) const {
    InputRouteResult result{};

    // Layer 1: Lifecycle and System events.
    switch (translated.systemSignal) {
    case SystemSignal::none:
        break;
    case SystemSignal::quit:
        result.quitRequested = true;
        return result;
    case SystemSignal::enterBackground:
        result.enteredBackground = true;
        return result;
    case SystemSignal::enterForeground:
        result.enteredForeground = true;
        return result;
    case SystemSignal::invalidateGraphics:
        result.invalidateGraphics = true;
        return result;
    }

    // Layer 2: Tools (DebugEditor) - navigation/control keys and text input.
    bool navConsumed = false;
    if (translated.debugKey && debugEditor.onKey(*translated.debugKey, game)) {
        result.consumedByDebug = true;
        navConsumed = true;
    }

    if (translated.textInput) {
        if (debugEditor.onTextInput(*translated.textInput, game)) {
            result.consumedByDebug = true;
        } else {
            // Layer 3: Gameplay UI text input.
            bool uiConsumedText = false;
            for (ViewportUiConsumer* consumer : uiConsumers_) {
                if (consumer != nullptr && consumer->onTextInput(*translated.textInput)) {
                    uiConsumedText = true;
                    result.consumedByUi = true;
                    break;
                }
            }
            // Layer 4: World text input.
            if (!uiConsumedText) {
                game.onTextInput(*translated.textInput);
            }
        }
    }

    // Layer 3: Gameplay UI directional/confirm/cancel navigation.
    if (!navConsumed && translated.navAction) {
        for (ViewportUiConsumer* consumer : uiConsumers_) {
            if (consumer != nullptr && consumer->onNavigation(*translated.navAction)) {
                result.consumedByUi = true;
                break;
            }
        }
    }

    // Viewport Pointer & Primary Press routing (Viewport -> Tools -> Gameplay UI -> World).
    if (translated.pointerMoved || translated.primaryPressed) {
        bool pointerHoverConsumedByUi = false;
        if (translated.pointerMoved) {
            for (ViewportUiConsumer* consumer : uiConsumers_) {
                if (consumer != nullptr
                    && consumer->onPointerMove(translated.viewportPoint,
                                               translated.insideViewport)) {
                    pointerHoverConsumedByUi = true;
                    result.consumedByUi = true;
                    break;
                }
            }
            const SDL_FPoint worldPoint = game.camera().viewportToWorld(
                    translated.viewportPoint, interpolation);
            game.onPointerMove(worldPoint,
                               translated.insideViewport && !pointerHoverConsumedByUi);
        }

        if (translated.primaryPressed && translated.insideViewport) {
            // Layer 2: DebugEditor viewport panel hit-test.
            if (debugEditor.consumesViewportPoint(translated.viewportPoint)) {
                result.consumedByDebug = true;
                return result;
            }

            // Layer 3: Future Gameplay UIs in logical viewport (1280x720) space.
            for (ViewportUiConsumer* consumer : uiConsumers_) {
                if (consumer != nullptr && consumer->onPrimaryPress(translated.viewportPoint)) {
                    result.consumedByUi = true;
                    return result;
                }
            }

            // Layer 4: World click after camera offset conversion.
            const SDL_FPoint worldPoint = game.camera().viewportToWorld(
                    translated.viewportPoint, interpolation);
            game.onClick(worldPoint);
        }
    }

    return result;
}

InputRouteResult InputRouter::handleEvent(const SDL_Event& event,
                                          InputSystem& input,
                                          DebugEditor& debugEditor,
                                          Game& game,
                                          const ViewportCoordinateMapper& mapper,
                                          const float interpolation) const {
    const TranslatedInput translated = input.translateEvent(event, mapper);
    return routeTranslated(translated, debugEditor, game, interpolation);
}

void InputRouter::update(const float deltaSeconds,
                         InputSystem& input,
                         DebugEditor& debugEditor,
                         Game& game,
                         const float interpolation) const {
    if (const auto cursorStep = input.advanceCursor(deltaSeconds)) {
        (void)routeTranslated(*cursorStep, debugEditor, game, interpolation);
    }
}
