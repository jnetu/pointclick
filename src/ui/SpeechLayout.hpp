#pragma once

#include "ui/TextLayout.hpp"

#include <SDL3/SDL_rect.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Where a spoken line goes on screen, in viewport space: wrapped by words to kMaxWidth, centered
// above the speaker's head and kept inside the viewport. The optional speaker name sits on its own
// line on top (FontRole::speaker). Wrapping is cached and redone only when the text, the speaker or
// the metrics change; placement is cheap and follows the anchor every frame.
class SpeechLayout {
public:
    static constexpr float kMaxWidth = 820.0F;
    static constexpr float kViewportMargin = 16.0F;
    static constexpr float kGapAboveHead = 14.0F;

    // Wraps again only when something changed. Returns true when it did.
    bool update(std::string_view text, std::string_view speaker, const TextMetrics& metrics);
    // Moves the block so its bottom-center sits at anchor (minus the gap), inside the viewport.
    void place(SDL_FPoint anchor);

    [[nodiscard]] const std::string& text() const { return text_; }
    [[nodiscard]] const std::string& speaker() const { return speaker_; }
    [[nodiscard]] const std::vector<TextLine>& lines() const { return lines_; }
    [[nodiscard]] SDL_FRect bounds() const { return bounds_; }
    // Top-left of the speaker name and of each line, each centered in bounds().
    [[nodiscard]] SDL_FPoint speakerOrigin() const;
    [[nodiscard]] SDL_FPoint lineOrigin(std::size_t index) const;

private:
    std::string text_;
    std::string speaker_;
    std::uint32_t revision_ = 0;
    bool wrapped_ = false;
    std::vector<TextLine> lines_;
    float lineHeight_ = 0.0F;
    float speakerWidth_ = 0.0F;
    float speakerHeight_ = 0.0F;
    SDL_FPoint size_{};
    SDL_FRect bounds_{};
};
