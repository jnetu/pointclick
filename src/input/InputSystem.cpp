#include "input/InputSystem.hpp"
#include "game/World.hpp"

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_touch.h>

#include <algorithm>
#include <cmath>

namespace {
bool identityWindowToViewport(const void*, const float windowX, const float windowY,
                              SDL_FPoint& viewportOut) {
    viewportOut = {windowX, windowY};
    return true;
}

bool identityTouchToViewport(const void*, const float normalizedX, const float normalizedY,
                             SDL_FPoint& viewportOut) {
    viewportOut = {
        normalizedX * static_cast<float>(World::width),
        normalizedY * static_cast<float>(World::height),
    };
    return true;
}

float normalizeAxisValue(const Sint16 raw) noexcept {
    return raw < 0 ? static_cast<float>(raw) / 32768.0F
                   : static_cast<float>(raw) / 32767.0F;
}

bool isIgnoredTouchEvent(const SDL_TouchFingerEvent& tfinger) {
    if (tfinger.touchID == SDL_MOUSE_TOUCHID) {
        return true;
    }
    if (tfinger.touchID != 0) {
        const SDL_TouchDeviceType type = SDL_GetTouchDeviceType(tfinger.touchID);
        if (type == SDL_TOUCH_DEVICE_INDIRECT_ABSOLUTE
            || type == SDL_TOUCH_DEVICE_INDIRECT_RELATIVE) {
            return true;
        }
    }
    return false;
}
}

ViewportCoordinateMapper ViewportCoordinateMapper::identity() noexcept {
    return {
        .context = nullptr,
        .windowToViewport = &identityWindowToViewport,
        .touchToViewport = &identityTouchToViewport,
    };
}

bool ViewportCoordinateMapper::mapWindow(const float windowX, const float windowY,
                                         SDL_FPoint& viewportOut) const {
    if (windowToViewport) {
        return windowToViewport(context, windowX, windowY, viewportOut);
    }
    return identityWindowToViewport(nullptr, windowX, windowY, viewportOut);
}

bool ViewportCoordinateMapper::mapTouch(const float normalizedX, const float normalizedY,
                                        SDL_FPoint& viewportOut) const {
    if (touchToViewport) {
        return touchToViewport(context, normalizedX, normalizedY, viewportOut);
    }
    return identityTouchToViewport(nullptr, normalizedX, normalizedY, viewportOut);
}

InputSystem::InputSystem()
        : cursorViewport_{static_cast<float>(World::width) * 0.5F,
                          static_cast<float>(World::height) * 0.5F} {}

InputSystem::~InputSystem() {
    shutdown();
}

void InputSystem::initialize() {
    if (!gamepadSubsystemInitialized_) {
        if (SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
            gamepadSubsystemInitialized_ = true;
            int count = 0;
            if (SDL_JoystickID* ids = SDL_GetGamepads(&count)) {
                for (int i = 0; i < count; ++i) {
                    openGamepad(ids[i]);
                }
                SDL_free(ids);
            }
        } else {
            SDL_Log("Gamepad subsystem unavailable: %s", SDL_GetError());
        }
    }
}

void InputSystem::shutdown() {
    for (const TrackedGamepad& pad : gamepads_) {
        if (pad.handle != nullptr) {
            SDL_CloseGamepad(pad.handle);
        }
    }
    gamepads_.clear();
    activeGamepadId_ = 0;
    resetTransientState();

    if (gamepadSubsystemInitialized_) {
        SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
        gamepadSubsystemInitialized_ = false;
    }
}

void InputSystem::resetTransientState() noexcept {
    rawStickX_ = 0;
    rawStickY_ = 0;
    moveAxis_ = {0.0F, 0.0F};
    activeFinger_.reset();
}

void InputSystem::setCursorViewport(const SDL_FPoint viewportPoint) noexcept {
    cursorViewport_ = World::clampToViewport(viewportPoint);
}

void InputSystem::setDeadzone(const float deadzone) noexcept {
    deadzone_ = std::clamp(deadzone, 0.0F, 0.95F);
    updateMoveAxis();
}

void InputSystem::setCursorSpeed(const float pixelsPerSecond) noexcept {
    cursorSpeed_ = std::max(pixelsPerSecond, 0.0F);
}

void InputSystem::openGamepad(const SDL_JoystickID id) {
    const auto existing = std::find_if(gamepads_.begin(), gamepads_.end(),
                                       [id](const TrackedGamepad& pad) { return pad.id == id; });
    if (existing != gamepads_.end()) {
        return;
    }
    SDL_Gamepad* handle = SDL_WasInit(SDL_INIT_GAMEPAD) ? SDL_OpenGamepad(id) : nullptr;
    gamepads_.push_back({id, handle});
    if (activeGamepadId_ == 0) {
        activeGamepadId_ = id;
    }
}

void InputSystem::closeGamepad(const SDL_JoystickID id) noexcept {
    const auto found = std::find_if(gamepads_.begin(), gamepads_.end(),
                                    [id](const TrackedGamepad& pad) { return pad.id == id; });
    if (found != gamepads_.end()) {
        if (found->handle != nullptr) {
            SDL_CloseGamepad(found->handle);
        }
        gamepads_.erase(found);
    }
    if (gamepads_.empty() || activeGamepadId_ == id) {
        activeGamepadId_ = gamepads_.empty() ? 0 : gamepads_.front().id;
        rawStickX_ = 0;
        rawStickY_ = 0;
        moveAxis_ = {0.0F, 0.0F};
    }
}

void InputSystem::updateMoveAxis() noexcept {
    const float x = normalizeAxisValue(rawStickX_);
    const float y = normalizeAxisValue(rawStickY_);
    const float magnitude = std::hypot(x, y);
    if (magnitude <= deadzone_) {
        moveAxis_ = {0.0F, 0.0F};
        return;
    }
    const float clampedMagnitude = std::min(magnitude, 1.0F);
    const float scaledMagnitude = (clampedMagnitude - deadzone_) / (1.0F - deadzone_);
    const float invMagnitude = 1.0F / magnitude;
    moveAxis_ = {x * invMagnitude * scaledMagnitude, y * invMagnitude * scaledMagnitude};
}

std::optional<TranslatedInput> InputSystem::advanceCursor(const float deltaSeconds) noexcept {
    if (deltaSeconds <= 0.0F || (moveAxis_.x == 0.0F && moveAxis_.y == 0.0F)) {
        return std::nullopt;
    }
    cursorViewport_ = World::clampToViewport({
        cursorViewport_.x + moveAxis_.x * cursorSpeed_ * deltaSeconds,
        cursorViewport_.y + moveAxis_.y * cursorSpeed_ * deltaSeconds,
    });
    activeDevice_ = InputDevice::gamepad;
    TranslatedInput out;
    out.pointerMoved = true;
    out.viewportPoint = cursorViewport_;
    out.insideViewport = true;
    return out;
}

TranslatedInput InputSystem::translateEvent(const SDL_Event& event,
                                            const ViewportCoordinateMapper& mapper) {
    TranslatedInput out;

    switch (event.type) {
    case SDL_EVENT_QUIT:
    case SDL_EVENT_TERMINATING:
        out.systemSignal = SystemSignal::quit;
        break;

    case SDL_EVENT_WILL_ENTER_BACKGROUND:
    case SDL_EVENT_DID_ENTER_BACKGROUND:
        resetTransientState();
        out.systemSignal = SystemSignal::enterBackground;
        break;

    case SDL_EVENT_WILL_ENTER_FOREGROUND:
    case SDL_EVENT_DID_ENTER_FOREGROUND:
        out.systemSignal = SystemSignal::enterForeground;
        break;

    case SDL_EVENT_RENDER_DEVICE_RESET:
    case SDL_EVENT_RENDER_TARGETS_RESET:
    case SDL_EVENT_LOW_MEMORY:
        out.systemSignal = SystemSignal::invalidateGraphics;
        break;

    case SDL_EVENT_WINDOW_FOCUS_LOST:
        resetTransientState();
        break;

    case SDL_EVENT_TEXT_INPUT:
        activeDevice_ = InputDevice::mouseKeyboard;
        out.textInput.emplace(event.text.text != nullptr ? event.text.text : "");
        break;

    case SDL_EVENT_KEY_DOWN:
        activeDevice_ = InputDevice::mouseKeyboard;
        switch (event.key.key) {
        case SDLK_F1:
            if (!event.key.repeat) out.debugKey = DebugKey::toggleEditor;
            break;
        case SDLK_UP:
            out.debugKey = DebugKey::previous;
            out.navAction = UiNavAction::up;
            break;
        case SDLK_DOWN:
            out.debugKey = DebugKey::next;
            out.navAction = UiNavAction::down;
            break;
        case SDLK_LEFT:
            out.debugKey = DebugKey::decrease;
            out.navAction = UiNavAction::left;
            break;
        case SDLK_RIGHT:
            out.debugKey = DebugKey::increase;
            out.navAction = UiNavAction::right;
            break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            out.debugKey = DebugKey::submit;
            out.navAction = UiNavAction::confirm;
            break;
        case SDLK_BACKSPACE:
            out.debugKey = DebugKey::backspace;
            break;
        case SDLK_ESCAPE:
            out.debugKey = DebugKey::close;
            out.navAction = UiNavAction::cancel;
            break;
        case SDLK_TAB:
            if (!event.key.repeat) out.debugKey = DebugKey::togglePanel;
            break;
        default:
            break;
        }
        break;

    case SDL_EVENT_MOUSE_MOTION:
        if (event.motion.which == SDL_TOUCH_MOUSEID) break;
        if (mapper.mapWindow(event.motion.x, event.motion.y, out.viewportPoint)) {
            activeDevice_ = InputDevice::mouseKeyboard;
            out.pointerMoved = true;
            out.insideViewport = World::containsViewportPoint(out.viewportPoint);
            if (out.insideViewport) cursorViewport_ = out.viewportPoint;
        }
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (event.button.which == SDL_TOUCH_MOUSEID) break;
        if (mapper.mapWindow(event.button.x, event.button.y, out.viewportPoint)) {
            activeDevice_ = InputDevice::mouseKeyboard;
            out.pointerMoved = true;
            out.insideViewport = World::containsViewportPoint(out.viewportPoint);
            if (out.insideViewport) cursorViewport_ = out.viewportPoint;
            if (event.button.button == SDL_BUTTON_LEFT) out.primaryPressed = true;
        }
        break;

    case SDL_EVENT_FINGER_DOWN:
        if (isIgnoredTouchEvent(event.tfinger)) break;
        if (!activeFinger_ || *activeFinger_ == event.tfinger.fingerID) {
            activeFinger_ = event.tfinger.fingerID;
            if (mapper.mapTouch(event.tfinger.x, event.tfinger.y, out.viewportPoint)) {
                activeDevice_ = InputDevice::touch;
                out.pointerMoved = true;
                out.primaryPressed = true;
                out.insideViewport = World::containsViewportPoint(out.viewportPoint);
                if (out.insideViewport) cursorViewport_ = out.viewportPoint;
            }
        }
        break;

    case SDL_EVENT_FINGER_MOTION:
        if (isIgnoredTouchEvent(event.tfinger)) break;
        if (!activeFinger_ || *activeFinger_ == event.tfinger.fingerID) {
            if (mapper.mapTouch(event.tfinger.x, event.tfinger.y, out.viewportPoint)) {
                activeDevice_ = InputDevice::touch;
                out.pointerMoved = true;
                out.insideViewport = World::containsViewportPoint(out.viewportPoint);
                if (out.insideViewport) cursorViewport_ = out.viewportPoint;
            }
        }
        break;

    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED:
        if (isIgnoredTouchEvent(event.tfinger)) break;
        if (activeFinger_ && *activeFinger_ == event.tfinger.fingerID) {
            activeFinger_.reset();
            if (event.type == SDL_EVENT_FINGER_UP
                && mapper.mapTouch(event.tfinger.x, event.tfinger.y, out.viewportPoint)) {
                activeDevice_ = InputDevice::touch;
                out.pointerMoved = true;
                out.insideViewport = World::containsViewportPoint(out.viewportPoint);
                if (out.insideViewport) cursorViewport_ = out.viewportPoint;
            }
        }
        break;

    case SDL_EVENT_GAMEPAD_ADDED:
        openGamepad(event.gdevice.which);
        break;

    case SDL_EVENT_GAMEPAD_REMOVED:
        closeGamepad(event.gdevice.which);
        break;

    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        if (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX
            || event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY) {
            activeGamepadId_ = event.gaxis.which;
            if (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX) rawStickX_ = event.gaxis.value;
            else rawStickY_ = event.gaxis.value;
            updateMoveAxis();
            if (moveAxis_.x != 0.0F || moveAxis_.y != 0.0F) {
                activeDevice_ = InputDevice::gamepad;
            }
        }
        break;

    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        activeDevice_ = InputDevice::gamepad;
        activeGamepadId_ = event.gbutton.which;
        switch (event.gbutton.button) {
        case SDL_GAMEPAD_BUTTON_SOUTH:
            out.pointerMoved = true;
            out.primaryPressed = true;
            out.viewportPoint = cursorViewport_;
            out.insideViewport = World::containsViewportPoint(cursorViewport_);
            break;
        case SDL_GAMEPAD_BUTTON_BACK:
            out.debugKey = DebugKey::toggleEditor;
            break;
        case SDL_GAMEPAD_BUTTON_DPAD_UP:
            out.debugKey = DebugKey::previous;
            out.navAction = UiNavAction::up;
            break;
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
            out.debugKey = DebugKey::next;
            out.navAction = UiNavAction::down;
            break;
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
            out.debugKey = DebugKey::decrease;
            out.navAction = UiNavAction::left;
            break;
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
            out.debugKey = DebugKey::increase;
            out.navAction = UiNavAction::right;
            break;
        case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
            out.debugKey = DebugKey::decrease;
            break;
        case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
            out.debugKey = DebugKey::increase;
            break;
        case SDL_GAMEPAD_BUTTON_WEST:
            out.debugKey = DebugKey::togglePanel;
            break;
        case SDL_GAMEPAD_BUTTON_EAST:
            out.debugKey = DebugKey::close;
            out.navAction = UiNavAction::cancel;
            break;
        case SDL_GAMEPAD_BUTTON_START:
            out.debugKey = DebugKey::submit;
            out.navAction = UiNavAction::confirm;
            break;
        default:
            break;
        }
        break;

    default:
        break;
    }

    return out;
}
