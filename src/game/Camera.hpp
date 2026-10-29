#pragma once

#include <SDL3/SDL_rect.h>

// 2D camera in logical world units. Its field of view is always World::width x World::height.
class Camera {
public:
    Camera() = default;
    Camera(float roomWidth, float roomHeight);

    void configure(float roomWidth, float roomHeight);
    void snapTo(SDL_FPoint focusFeet);
    void tick(float deltaSeconds, SDL_FPoint focusFeet);

    [[nodiscard]] SDL_FPoint position() const;
    [[nodiscard]] SDL_FPoint interpolatedPosition(float alpha) const;
    [[nodiscard]] SDL_FPoint targetFor(SDL_FPoint focusFeet) const;
    [[nodiscard]] SDL_FRect viewportBounds(float alpha = 1.0F) const;
    [[nodiscard]] SDL_FPoint viewportToWorld(SDL_FPoint viewportPoint, float alpha = 1.0F) const;
    [[nodiscard]] SDL_FPoint worldToViewport(SDL_FPoint worldPoint, float alpha = 1.0F) const;

private:
    static constexpr float kSmoothingSpeed = 6.0F;
    static constexpr float kSnapEpsilon = 0.05F;

    [[nodiscard]] SDL_FPoint clamp(SDL_FPoint position) const;

    float roomWidth_ = 1280.0F;
    float roomHeight_ = 720.0F;
    SDL_FPoint previousPosition_{0.0F, 0.0F};
    SDL_FPoint position_{0.0F, 0.0F};
};
