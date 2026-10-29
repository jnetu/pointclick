#include "core/Application.hpp"
#include "content/InteractionFiles.hpp"
#include "content/RoomFiles.hpp"
#include "content/RoomPresets.hpp"
#include "game/Identifier.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <string>
#include <thread>
#include <utility>

namespace {
constexpr double kTicksPerSecond = 20.0;
constexpr double kFramesPerSecond = 60.0;
constexpr double kTickSeconds = 1.0 / kTicksPerSecond;
constexpr double kMaximumFrameTime = 0.25;
}

Application::Application(std::filesystem::path roomDirectory)
        : game_(RoomPresets::firstRoom()) {
    debugEditor_.setRoomDirectory(std::move(roomDirectory));
}

int Application::run() {
    if (!initialize()) {
        shutdown();
        return 1;
    }

    mainLoop();
    shutdown();
    return 0;
}

bool Application::loadRoomWithRules(const std::string_view roomFilename) {
    const auto roomPath = debugEditor_.roomDirectory() / std::string(roomFilename);
    auto loaded = RoomFiles::load(roomPath);
    if (!loaded.room) {
        SDL_Log("Could not load room: %s", loaded.error.c_str());
        return false;
    }
    auto rulesLoaded = InteractionFiles::loadOptionalForRoom(roomPath, &*loaded.room);
    if (!rulesLoaded.interactions) {
        SDL_Log("Could not load room rules: %s", rulesLoaded.error.c_str());
        return false;
    }
    std::string roomError;
    if (!game_.applyRoom(std::move(*loaded.room), true, roomError)) {
        SDL_Log("Could not apply room: %s", roomError.c_str());
        return false;
    }
    std::string rulesError;
    if (!game_.setInteractions(std::move(*rulesLoaded.interactions), rulesError)) {
        SDL_Log("Could not apply room rules: %s", rulesError.c_str());
        return false;
    }
    return true;
}

void Application::processPendingRoomTransition() {
    const auto nextRoomId = game_.consumeRoomTransition();
    if (!nextRoomId) return;
    if (!validIdentifier(*nextRoomId)) {
        SDL_Log("Invalid room transition target: %s", nextRoomId->c_str());
        return;
    }
    (void)loadRoomWithRules(*nextRoomId + ".room");
}

bool Application::initialize() {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_Log("Could not initialize SDL: %s", SDL_GetError());
        return false;
    }
    inputSystem_.initialize();

    window_ = SDL_CreateWindow(
            "Point & Click",
            1280,
            720,
            SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
    );

    if (window_ == nullptr) {
        SDL_Log("Could not create the window: %s", SDL_GetError());
        return false;
    }

    if (!renderer_.initialize(window_)) {
        return false;
    }

    if (debugEditor_.roomDirectory().empty()) {
        const char* base = SDL_GetBasePath();
        if (!base) { SDL_Log("Could not locate assets: %s", SDL_GetError()); return false; }
        // SDL owns this cached UTF-8 path; on Android it is "./" for APK assets.
        const std::u8string basePath(base, base + SDL_strlen(base));
        debugEditor_.setRoomDirectory(std::filesystem::path(basePath) / "assets" / "rooms");
    }
    renderer_.setSpriteRoot(debugEditor_.roomDirectory().parent_path() / "sprites");
    if (!loadRoomWithRules("first.room")) {
        return false;
    }

    // Avoid opening the on-screen virtual keyboard immediately on mobile/console boot.
    if (!SDL_HasScreenKeyboardSupport() && !SDL_StartTextInput(window_)) {
        SDL_Log("Could not start text input: %s", SDL_GetError());
        return false;
    }

    running_ = true;
    return true;
}

void Application::mainLoop() {
    using Clock = std::chrono::steady_clock;

    const auto renderStep = std::chrono::duration_cast<Clock::duration>(
            std::chrono::duration<double>(1.0 / kFramesPerSecond)
    );

    auto previousTime = Clock::now();
    auto nextRenderTime = previousTime;
    double tickAccumulator = 0.0;

    while (running_) {
        processEvents();
        if (!running_) {
            break;
        }
        processPendingRoomTransition();
        if (suspended_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }
        if (resetFrameClock_) {
            previousTime = Clock::now();
            nextRenderTime = previousTime;
            tickAccumulator = 0.0;
            resetFrameClock_ = false;
        }

        const auto currentTime = Clock::now();
        const double elapsedSeconds = std::chrono::duration<double>(
                currentTime - previousTime
        ).count();
        previousTime = currentTime;

        const double frameSeconds = std::min(elapsedSeconds, kMaximumFrameTime);
        tickAccumulator += frameSeconds;

        while (tickAccumulator >= kTickSeconds) {
            game_.tick(static_cast<float>(kTickSeconds));
            processPendingRoomTransition();
            tickAccumulator -= kTickSeconds;
        }

        renderInterpolation_ = static_cast<float>(tickAccumulator / kTickSeconds);
        inputRouter_.update(static_cast<float>(frameSeconds), inputSystem_,
                            debugEditor_, game_, renderInterpolation_);

        if (currentTime >= nextRenderTime) {
            renderer_.render(game_, renderInterpolation_, &debugEditor_, &inputSystem_);

            nextRenderTime += renderStep;
            if (nextRenderTime < currentTime) {
                nextRenderTime = currentTime + renderStep;
            }
        } else {
            std::this_thread::sleep_until(nextRenderTime);
        }
    }
}

void Application::processEvents() {
    const ViewportCoordinateMapper mapper = renderer_.viewportMapper();
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        const InputRouteResult result = inputRouter_.handleEvent(
                event, inputSystem_, debugEditor_, game_, mapper, renderInterpolation_);
        if (result.quitRequested) {
            running_ = false;
        }
        if (result.enteredBackground) {
            suspended_ = true;
        }
        if (result.enteredForeground) {
            suspended_ = false;
            resetFrameClock_ = true;
        }
        if (result.invalidateGraphics) {
            renderer_.invalidateTextures();
        }
    }
    syncTextInputState();
}

void Application::syncTextInputState() {
    if (window_ == nullptr || !SDL_HasScreenKeyboardSupport()) {
        return;
    }
    const bool wantTextInput = debugEditor_.panelVisible()
                               && inputSystem_.activeDevice() != InputDevice::gamepad;
    if (wantTextInput && !SDL_TextInputActive(window_)) {
        SDL_StartTextInput(window_);
    } else if (!wantTextInput && SDL_TextInputActive(window_)) {
        SDL_StopTextInput(window_);
    }
}

void Application::shutdown() {
    if (window_ != nullptr && SDL_TextInputActive(window_)) {
        SDL_StopTextInput(window_);
    }

    inputSystem_.shutdown();
    renderer_.shutdown();

    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }

    SDL_Quit();
}
