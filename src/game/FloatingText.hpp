#pragma once

#include <string>
#include <string_view>

// A spoken line or remark and how long it stays up. Where it is drawn is presentation: graphics/
// anchors it above the speaker (ui/SpeechLayout); the domain only keeps text and time.
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
    // Who says it, for the name above the line; empty until `say` gets a speaker (ROADMAP M1).
    [[nodiscard]] const std::string& speaker() const;

private:
    static constexpr float kLifetimeSeconds = 10.0F;

    std::string text_;
    std::string speaker_;
    float remainingSeconds_ = 0.0F;
    bool visible_ = false;
};
