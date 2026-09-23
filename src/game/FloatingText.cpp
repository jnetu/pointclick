#include "game/FloatingText.hpp"

#include <algorithm>
#include <cmath>

void FloatingText::show(const std::string_view text) {
    text_ = text;
    previousX_ = 20.0F;
    x_ = 20.0F;
    remainingSeconds_ = kLifetimeSeconds;
    visible_ = !text_.empty();
}

void FloatingText::tick(const float deltaSeconds) {
    if (!visible_) {
        return;
    }

    previousX_ = x_;
    x_ += kSpeedPixelsPerSecond * deltaSeconds;
    remainingSeconds_ -= deltaSeconds;

    if (remainingSeconds_ <= 0.0F) {
        visible_ = false;
    }
}

bool FloatingText::isVisible() const {
    return visible_;
}

const std::string& FloatingText::text() const {
    return text_;
}

float FloatingText::interpolatedX(const float alpha) const {
    return std::lerp(previousX_, x_, std::clamp(alpha, 0.0F, 1.0F));
}

float FloatingText::y() const {
    return y_;
}
