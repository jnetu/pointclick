#pragma once

#include "ui/TextMetrics.hpp"

#include <SDL3/SDL_rect.h>

#include <cstddef>
#include <string_view>
#include <vector>

// One wrapped line: a byte range of the source text (no copy) and its measured width.
struct TextLine {
    std::size_t begin = 0;
    std::size_t length = 0;
    float width = 0.0F;

    [[nodiscard]] std::string_view in(const std::string_view text) const {
        return text.substr(begin, length);
    }
};

namespace TextLayout {

// Breaks UTF-8 text into lines no wider than maxWidth, at spaces, and at every '\n'. A word wider
// than the line is split between code points (never inside one). Spaces at a break are dropped.
// Reuses `lines` (cleared first). An empty text gives no lines; an empty paragraph gives an empty
// line, so "a\n\nb" keeps its blank line.
void wrap(std::string_view text, float maxWidth, const TextMetrics& metrics, FontRole role,
          std::vector<TextLine>& lines);

[[nodiscard]] float widest(const std::vector<TextLine>& lines);

// A box of `size` centered horizontally on anchor.x with its bottom at anchor.y, moved as little
// as needed to fit inside `area` (when it is larger than the area, it starts at the area's edge).
[[nodiscard]] SDL_FRect placeAbove(SDL_FPoint size, SDL_FPoint anchor, SDL_FRect area);

} // namespace TextLayout
