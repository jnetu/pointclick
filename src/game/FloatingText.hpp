#pragma once

#include <string>
#include <string_view>

class FloatingText {
public:
    void show(std::string_view text);
    void show(std::string_view text, float seconds);
    void hide();
    void tick(float deltaSeconds);

    // Scripted lines stay up long enough to read, then let the sequence continue.
    [[nodiscard]] static float readingSeconds(std::string_view text);

    [[nodiscard]] bool isVisible() const;
    [[nodiscard]] const std::string& text() const;
    [[nodiscard]] float interpolatedX(float alpha) const;
    [[nodiscard]] float y() const;

private:
    static constexpr float kSpeedPixelsPerSecond = 5.0F;
    static constexpr float kLifetimeSeconds = 10.0F;

    std::string text_;
    float previousX_ = 20.0F;
    float x_ = 20.0F;
    float y_ = 100.0F;
    float remainingSeconds_ = 0.0F;
    bool visible_ = false;
};
