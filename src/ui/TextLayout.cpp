#include "ui/TextLayout.hpp"

#include <algorithm>

namespace {

[[nodiscard]] bool isContinuation(const char ch) {
    return (static_cast<unsigned char>(ch) & 0xC0U) == 0x80U;
}

// End of the code point starting at pos.
[[nodiscard]] std::size_t nextCodePoint(const std::string_view text, std::size_t pos) {
    ++pos;
    while (pos < text.size() && isContinuation(text[pos])) ++pos;
    return pos;
}

class LineBuilder {
public:
    LineBuilder(const std::string_view text, const float maxWidth, const TextMetrics& metrics,
                const FontRole role, std::vector<TextLine>& lines)
        : text_(text), maxWidth_(maxWidth), metrics_(metrics), role_(role), lines_(lines) {}

    [[nodiscard]] float measure(const std::size_t begin, const std::size_t end) const {
        return metrics_.width(role_, text_.substr(begin, end - begin));
    }

    void emit(const std::size_t begin, const std::size_t end) {
        lines_.push_back({begin, end - begin, measure(begin, end)});
    }

    // Splits [begin, end) (one word wider than the line) into pieces that fit, keeping at least
    // one code point per line. Returns where the unplaced rest begins.
    std::size_t splitWord(const std::size_t begin, const std::size_t end) {
        std::size_t start = begin;
        while (start < end) {
            std::size_t fit = nextCodePoint(text_, start);
            while (fit < end) {
                const std::size_t next = nextCodePoint(text_, fit);
                if (measure(start, next) > maxWidth_) break;
                fit = next;
            }
            if (fit >= end) return start; // the tail fits: it starts the next line
            emit(start, fit);
            start = fit;
        }
        return end;
    }

    void paragraph(const std::size_t begin, const std::size_t end) {
        std::size_t lineBegin = std::string_view::npos; // no word on the line yet
        std::size_t lineEnd = begin;
        std::size_t pos = begin;
        while (pos < end) {
            while (pos < end && text_[pos] == ' ') ++pos;
            if (pos >= end) break;
            std::size_t wordEnd = pos;
            while (wordEnd < end && text_[wordEnd] != ' ') ++wordEnd;

            if (lineBegin == std::string_view::npos) {
                lineBegin = measure(pos, wordEnd) > maxWidth_ ? splitWord(pos, wordEnd) : pos;
                lineEnd = wordEnd;
            } else if (measure(lineBegin, wordEnd) <= maxWidth_) {
                lineEnd = wordEnd;
            } else {
                emit(lineBegin, lineEnd);
                lineBegin = measure(pos, wordEnd) > maxWidth_ ? splitWord(pos, wordEnd) : pos;
                lineEnd = wordEnd;
            }
            pos = wordEnd;
        }
        if (lineBegin == std::string_view::npos) lines_.push_back({begin, 0, 0.0F});
        else emit(lineBegin, lineEnd);
    }

private:
    std::string_view text_;
    float maxWidth_;
    const TextMetrics& metrics_;
    FontRole role_;
    std::vector<TextLine>& lines_;
};

} // namespace

void TextLayout::wrap(const std::string_view text, const float maxWidth,
                      const TextMetrics& metrics, const FontRole role,
                      std::vector<TextLine>& lines) {
    lines.clear();
    if (text.empty()) return;
    LineBuilder builder(text, maxWidth, metrics, role, lines);
    std::size_t begin = 0;
    while (true) {
        const std::size_t newline = text.find('\n', begin);
        const std::size_t end = newline == std::string_view::npos ? text.size() : newline;
        std::size_t paragraphEnd = end;
        if (paragraphEnd > begin && text[paragraphEnd - 1] == '\r') --paragraphEnd;
        builder.paragraph(begin, paragraphEnd);
        if (newline == std::string_view::npos) break;
        begin = newline + 1;
    }
}

float TextLayout::widest(const std::vector<TextLine>& lines) {
    float result = 0.0F;
    for (const TextLine& line : lines) result = std::max(result, line.width);
    return result;
}

SDL_FRect TextLayout::placeAbove(const SDL_FPoint size, const SDL_FPoint anchor,
                                 const SDL_FRect area) {
    const auto fit = [](const float wanted, const float extent, const float lo, const float span) {
        if (extent >= span) return lo;
        return std::clamp(wanted, lo, lo + span - extent);
    };
    return {fit(anchor.x - size.x * 0.5F, size.x, area.x, area.w),
            fit(anchor.y - size.y, size.y, area.y, area.h), size.x, size.y};
}
