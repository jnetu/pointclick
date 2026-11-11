#include "game/FloatingText.hpp"

#include <algorithm>

void FloatingText::show(const std::string_view text) {
    show(text, kLifetimeSeconds);
}

void FloatingText::show(const std::string_view text, const float seconds) {
    text_ = text;
    remainingSeconds_ = seconds;
    visible_ = !text_.empty() && seconds > 0.0F;
}

void FloatingText::hide() {
    visible_ = false;
    remainingSeconds_ = 0.0F;
}

float FloatingText::readingSeconds(const std::string_view text) {
    // Count UTF-8 code points, not bytes, so accented lines are not held longer.
    const auto characters = static_cast<float>(std::count_if(
            text.begin(), text.end(), [](const char ch) { return (ch & 0xC0) != 0x80; }));
    return std::clamp(1.0F + characters * 0.06F, 2.0F, kLifetimeSeconds);
}

void FloatingText::tick(const float deltaSeconds) {
    if (!visible_) {
        return;
    }

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

const std::string& FloatingText::speaker() const {
    return speaker_;
}
