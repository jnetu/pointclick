#pragma once

#include "debug/DebugEditor.hpp"

#include <SDL3/SDL_events.h>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

enum class InputDevice {
    mouseKeyboard,
    touch,
    gamepad,
};

enum class SystemSignal {
    none,
    quit,
    enterBackground,
    enterForeground,
    invalidateGraphics,
};

enum class UiNavAction {
    up,
    down,
    left,
    right,
    confirm,
    cancel,
};

// Maps raw window coordinates and normalized [0..1] touch coordinates into the
// fixed 1280x720 logical viewport without coupling input/ to graphics/.
struct ViewportCoordinateMapper {
    const void* context = nullptr;
    bool (*windowToViewport)(const void* context, float windowX, float windowY,
                             SDL_FPoint& viewportOut) = nullptr;
    bool (*touchToViewport)(const void* context, float normalizedX, float normalizedY,
                            SDL_FPoint& viewportOut) = nullptr;

    [[nodiscard]] static ViewportCoordinateMapper identity() noexcept;
    [[nodiscard]] bool mapWindow(float windowX, float windowY, SDL_FPoint& viewportOut) const;
    [[nodiscard]] bool mapTouch(float normalizedX, float normalizedY,
                                SDL_FPoint& viewportOut) const;
};

struct TranslatedInput {
    SystemSignal systemSignal = SystemSignal::none;
    std::optional<DebugKey> debugKey;
    std::optional<UiNavAction> navAction;
    std::optional<std::string> textInput;
    bool pointerMoved = false;
    bool primaryPressed = false;
    SDL_FPoint viewportPoint{0.0F, 0.0F};
    bool insideViewport = false;
};

// Translates raw SDL_Event streams (Mouse/Keyboard, Touch, Gamepad, Lifecycle)
// into device-agnostic viewport and UI/debug commands.
class InputSystem {
public:
    static constexpr float kDefaultDeadzone = 0.24F;
    static constexpr float kDefaultCursorSpeed = 540.0F;

    InputSystem();
    ~InputSystem();
    InputSystem(const InputSystem&) = delete;
    InputSystem& operator=(const InputSystem&) = delete;

    void initialize();
    void shutdown();
    void resetTransientState() noexcept;

    [[nodiscard]] TranslatedInput translateEvent(
            const SDL_Event& event,
            const ViewportCoordinateMapper& mapper = ViewportCoordinateMapper::identity());

    // Advances the gamepad virtual cursor inside the 1280x720 logical viewport.
    [[nodiscard]] std::optional<TranslatedInput> advanceCursor(float deltaSeconds) noexcept;

    [[nodiscard]] InputDevice activeDevice() const noexcept { return activeDevice_; }
    [[nodiscard]] SDL_FPoint moveAxis() const noexcept { return moveAxis_; }
    [[nodiscard]] SDL_FPoint cursorViewport() const noexcept { return cursorViewport_; }
    void setCursorViewport(SDL_FPoint viewportPoint) noexcept;

    [[nodiscard]] float deadzone() const noexcept { return deadzone_; }
    void setDeadzone(float deadzone) noexcept;
    [[nodiscard]] float cursorSpeed() const noexcept { return cursorSpeed_; }
    void setCursorSpeed(float pixelsPerSecond) noexcept;

    [[nodiscard]] std::size_t gamepadCount() const noexcept { return gamepads_.size(); }

private:
    struct TrackedGamepad {
        SDL_JoystickID id = 0;
        SDL_Gamepad* handle = nullptr;
    };

    void openGamepad(SDL_JoystickID id);
    void closeGamepad(SDL_JoystickID id) noexcept;
    void updateMoveAxis() noexcept;

    std::vector<TrackedGamepad> gamepads_;
    SDL_JoystickID activeGamepadId_ = 0;
    Sint16 rawStickX_ = 0;
    Sint16 rawStickY_ = 0;
    SDL_FPoint moveAxis_{0.0F, 0.0F};
    SDL_FPoint cursorViewport_{0.0F, 0.0F};
    float deadzone_ = kDefaultDeadzone;
    float cursorSpeed_ = kDefaultCursorSpeed;
    std::optional<SDL_FingerID> activeFinger_;
    InputDevice activeDevice_ = InputDevice::mouseKeyboard;
    bool gamepadSubsystemInitialized_ = false;
};
