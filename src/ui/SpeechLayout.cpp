#include "ui/SpeechLayout.hpp"

#include "game/World.hpp"

#include <algorithm>

bool SpeechLayout::update(const std::string_view text, const std::string_view speaker,
                          const TextMetrics& metrics) {
    if (wrapped_ && revision_ == metrics.revision() && text_ == text && speaker_ == speaker) {
        return false;
    }
    text_.assign(text);
    speaker_.assign(speaker);
    revision_ = metrics.revision();
    wrapped_ = true;

    const float maxWidth = std::min(kMaxWidth, static_cast<float>(World::width)
                                                       - kViewportMargin * 2.0F);
    TextLayout::wrap(text_, maxWidth, metrics, FontRole::dialogue, lines_);
    lineHeight_ = metrics.lineHeight(FontRole::dialogue);
    speakerWidth_ = speaker_.empty() ? 0.0F : metrics.width(FontRole::speaker, speaker_);
    speakerHeight_ = speaker_.empty() ? 0.0F : metrics.lineHeight(FontRole::speaker);
    size_ = {std::max(TextLayout::widest(lines_), speakerWidth_),
             speakerHeight_ + lineHeight_ * static_cast<float>(lines_.size())};
    return true;
}

void SpeechLayout::place(const SDL_FPoint anchor) {
    const SDL_FRect area{kViewportMargin, kViewportMargin,
                         static_cast<float>(World::width) - kViewportMargin * 2.0F,
                         static_cast<float>(World::height) - kViewportMargin * 2.0F};
    bounds_ = TextLayout::placeAbove(size_, {anchor.x, anchor.y - kGapAboveHead}, area);
}

SDL_FPoint SpeechLayout::speakerOrigin() const {
    return {bounds_.x + (bounds_.w - speakerWidth_) * 0.5F, bounds_.y};
}

SDL_FPoint SpeechLayout::lineOrigin(const std::size_t index) const {
    const float width = index < lines_.size() ? lines_[index].width : 0.0F;
    return {bounds_.x + (bounds_.w - width) * 0.5F,
            bounds_.y + speakerHeight_ + lineHeight_ * static_cast<float>(index)};
}
