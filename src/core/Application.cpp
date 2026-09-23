#include "core/Application.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <thread>

namespace {
constexpr double kTicksPerSecond = 20.0;
constexpr double kFramesPerSecond = 60.0;
constexpr double kTickSeconds = 1.0 / kTicksPerSecond;
constexpr double kMaximumFrameTime = 0.25;
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

bool Application::initialize() {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_Log("Could not initialize SDL: %s", SDL_GetError());
        return false;
    }

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

    if (!SDL_StartTextInput(window_)) {
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

        const auto currentTime = Clock::now();
        const double elapsedSeconds = std::chrono::duration<double>(
                currentTime - previousTime
        ).count();
        previousTime = currentTime;

        tickAccumulator += std::min(elapsedSeconds, kMaximumFrameTime);

        while (tickAccumulator >= kTickSeconds) {
            game_.tick(static_cast<float>(kTickSeconds));
            tickAccumulator -= kTickSeconds;
        }

        if (currentTime >= nextRenderTime) {
            const float interpolation = static_cast<float>(
                    tickAccumulator / kTickSeconds
            );
            renderer_.render(game_, interpolation);

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
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            running_ = false;
        } else if (event.type == SDL_EVENT_TEXT_INPUT) {
            game_.onTextInput(event.text.text);
        } else if (event.type == SDL_EVENT_MOUSE_MOTION
                   || event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            SDL_FPoint point{};
            const float x = event.type == SDL_EVENT_MOUSE_MOTION ? event.motion.x : event.button.x;
            const float y = event.type == SDL_EVENT_MOUSE_MOTION ? event.motion.y : event.button.y;
            if (renderer_.windowToWorld(x, y, point)) {
                game_.onPointerMove(point);
                if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN
                    && event.button.button == SDL_BUTTON_LEFT) {
                    game_.onClick(point);
                }
            }
        }
    }
}

void Application::shutdown() {
    if (window_ != nullptr && SDL_TextInputActive(window_)) {
        SDL_StopTextInput(window_);
    }

    renderer_.shutdown();

    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }

    SDL_Quit();
}
