#include "game/Camera.hpp"

#include "game/World.hpp"

#include <algorithm>
#include <cmath>

Camera::Camera(const float roomWidth, const float roomHeight) {
    configure(roomWidth, roomHeight);
}

void Camera::configure(const float roomWidth, const float roomHeight) {
    roomWidth_ = std::max(roomWidth, static_cast<float>(World::width));
    roomHeight_ = std::max(roomHeight, static_cast<float>(World::height));
    position_ = clamp(position_);
    previousPosition_ = clamp(previousPosition_);
}

void Camera::snapTo(const SDL_FPoint focusFeet) {
    position_ = targetFor(focusFeet);
    previousPosition_ = position_;
}

void Camera::tick(const float deltaSeconds, const SDL_FPoint focusFeet) {
    previousPosition_ = position_;
    const SDL_FPoint target = targetFor(focusFeet);
    if (deltaSeconds <= 0.0F || !std::isfinite(deltaSeconds)) {
        return;
    }

    const float factor = std::clamp(1.0F - std::exp(-kSmoothingSpeed * deltaSeconds), 0.0F, 1.0F);
    position_ = clamp({std::lerp(position_.x, target.x, factor),
                       std::lerp(position_.y, target.y, factor)});
    if (std::abs(target.x - position_.x) < kSnapEpsilon) {
        position_.x = target.x;
    }
    if (std::abs(target.y - position_.y) < kSnapEpsilon) {
        position_.y = target.y;
    }
}

SDL_FPoint Camera::position() const {
    return position_;
}

SDL_FPoint Camera::interpolatedPosition(const float alpha) const {
    const float t = std::clamp(alpha, 0.0F, 1.0F);
    return clamp({std::lerp(previousPosition_.x, position_.x, t),
                  std::lerp(previousPosition_.y, position_.y, t)});
}

SDL_FPoint Camera::targetFor(const SDL_FPoint focusFeet) const {
    if (!std::isfinite(focusFeet.x) || !std::isfinite(focusFeet.y)) {
        return position_;
    }
    return clamp({focusFeet.x - static_cast<float>(World::width) * 0.5F,
                  focusFeet.y - static_cast<float>(World::height) * 0.5F});
}

SDL_FRect Camera::viewportBounds(const float alpha) const {
    const SDL_FPoint offset = interpolatedPosition(alpha);
    return {offset.x, offset.y,
            static_cast<float>(World::width), static_cast<float>(World::height)};
}

SDL_FPoint Camera::viewportToWorld(const SDL_FPoint viewportPoint, const float alpha) const {
    const SDL_FPoint offset = interpolatedPosition(alpha);
    return {viewportPoint.x + offset.x, viewportPoint.y + offset.y};
}

SDL_FPoint Camera::worldToViewport(const SDL_FPoint worldPoint, const float alpha) const {
    const SDL_FPoint offset = interpolatedPosition(alpha);
    return {worldPoint.x - offset.x, worldPoint.y - offset.y};
}

SDL_FPoint Camera::clamp(const SDL_FPoint candidate) const {
    const float maxX = std::max(0.0F, roomWidth_ - static_cast<float>(World::width));
    const float maxY = std::max(0.0F, roomHeight_ - static_cast<float>(World::height));
    return {std::clamp(candidate.x, 0.0F, maxX),
            std::clamp(candidate.y, 0.0F, maxY)};
}
