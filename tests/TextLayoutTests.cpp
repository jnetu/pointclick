// Text layout without a window or GPU: wrapping and placement use injected metrics.
#include "game/World.hpp"
#include "ui/SpeechLayout.hpp"
#include "ui/TextLayout.hpp"

#include <cassert>
#include <string>
#include <string_view>
#include <vector>

namespace {

// 10 units per code point, 20 per line: widths are easy to count by hand.
constexpr MonospaceTextMetrics kMono{10.0F, 20.0F};

std::vector<std::string> wrapped(const std::string_view text, const float maxWidth,
                                 const TextMetrics& metrics = kMono) {
    std::vector<TextLine> lines;
    TextLayout::wrap(text, maxWidth, metrics, FontRole::dialogue, lines);
    std::vector<std::string> result;
    for (const TextLine& line : lines) {
        assert(line.width <= maxWidth || line.length > 0);
        result.emplace_back(line.in(text));
    }
    return result;
}

using Lines = std::vector<std::string>;

void testWrapsAtSpaces() {
    assert(wrapped("", 100.0F).empty());
    assert((wrapped("curto", 100.0F) == Lines{"curto"}));
    // "uma chave" = 9 code points = 90 <= 100; adding " de" would be 120.
    assert((wrapped("uma chave de latao", 100.0F) == Lines{"uma chave", "de latao"}));
    // Exactly the limit still fits.
    assert((wrapped("abcde fghij", 110.0F) == Lines{"abcde fghij"}));
    assert((wrapped("abcde fghij", 109.0F) == Lines{"abcde", "fghij"}));
    // Spaces at a break are dropped, inner runs are kept.
    assert((wrapped("  ab   cd  ", 40.0F) == Lines{"ab", "cd"}));
    assert((wrapped("ab  cd", 60.0F) == Lines{"ab  cd"}));
}

void testCountsCodePointsNotBytes() {
    // 8 code points (but 10 bytes): fits in 80 even though the bytes would not.
    assert((wrapped("Atenção!", 80.0F) == Lines{"Atenção!"}));
    assert((wrapped("Encontrei uma chave de latão escondida", 140.0F)
            == Lines{"Encontrei uma", "chave de latão", "escondida"}));
}

void testExplicitNewlines() {
    assert((wrapped("um\ndois", 500.0F) == Lines{"um", "dois"}));
    assert((wrapped("um\n\ndois", 500.0F) == Lines{"um", "", "dois"}));
    assert((wrapped("um\r\ndois\n", 500.0F) == Lines{"um", "dois", ""}));
}

void testSplitsWordsWiderThanTheLine() {
    // No space to break at: split between code points, never inside one.
    assert((wrapped("abcdefghij", 40.0F) == Lines{"abcd", "efgh", "ij"}));
    assert((wrapped("ok ãéíõúçàâêô fim", 40.0F)
            == Lines{"ok", "ãéíõ", "úçàâ", "êô", "fim"}));
    for (const std::string& line : wrapped("ãéíõúçàâêô", 30.0F)) {
        // Each piece is valid UTF-8: it never starts with a continuation byte.
        assert(!line.empty() && (static_cast<unsigned char>(line[0]) & 0xC0U) != 0x80U);
    }
    // Narrower than one glyph: still one code point per line, so the loop always advances.
    assert((wrapped("abc", 5.0F) == Lines{"a", "b", "c"}));
    // The rest of a split word shares its line with what follows when it fits.
    assert((wrapped("abcdef gh", 40.0F) == Lines{"abcd", "ef", "gh"}));
    assert((wrapped("abcdef g", 50.0F) == Lines{"abcde", "f g"}));
}

// Proportional metrics: the measure of a whole line, not a sum of fixed cells, decides.
class WideM final : public TextMetrics {
public:
    [[nodiscard]] float lineHeight(FontRole) const override { return 30.0F; }
    [[nodiscard]] float width(FontRole, const std::string_view text) const override {
        float total = 0.0F;
        for (const char ch : text) total += ch == 'M' ? 30.0F : 10.0F;
        return total;
    }
};

void testUsesTheInjectedMeasure() {
    const WideM metrics;
    assert((wrapped("MM ii", 70.0F, metrics) == Lines{"MM", "ii"}));
    assert((wrapped("ii ii", 70.0F, metrics) == Lines{"ii ii"}));
    std::vector<TextLine> lines;
    TextLayout::wrap("MM ii", 70.0F, metrics, FontRole::dialogue, lines);
    assert(lines.size() == 2 && lines[0].width == 60.0F && lines[1].width == 20.0F);
    assert(TextLayout::widest(lines) == 60.0F);
}

void testPlaceAboveStaysInside() {
    const SDL_FRect area{16.0F, 16.0F, 1248.0F, 688.0F};
    // Centered above the anchor.
    SDL_FRect box = TextLayout::placeAbove({200.0F, 60.0F}, {640.0F, 400.0F}, area);
    assert(box.x == 540.0F && box.y == 340.0F && box.w == 200.0F && box.h == 60.0F);
    // Near the left/right edges it slides in instead of being cut.
    box = TextLayout::placeAbove({200.0F, 60.0F}, {20.0F, 400.0F}, area);
    assert(box.x == 16.0F);
    box = TextLayout::placeAbove({200.0F, 60.0F}, {1275.0F, 400.0F}, area);
    assert(box.x + box.w == 1264.0F);
    // A head near the top pushes the text down to the margin.
    box = TextLayout::placeAbove({200.0F, 60.0F}, {640.0F, 30.0F}, area);
    assert(box.y == 16.0F);
    // Larger than the area: pinned to its start.
    box = TextLayout::placeAbove({2000.0F, 60.0F}, {640.0F, 400.0F}, area);
    assert(box.x == 16.0F);
}

void testSpeechLayout() {
    SpeechLayout layout;
    const std::string line(120, 'a'); // one 1200-unit "word": split to the 820 max width
    assert(layout.update(line, "", kMono));
    assert(!layout.update(line, "", kMono)); // same text and metrics: no re-wrap
    assert(layout.lines().size() == 2 && layout.lines()[0].width == 820.0F);
    layout.place({640.0F, 500.0F});
    const SDL_FRect bounds = layout.bounds();
    assert(bounds.w == 820.0F && bounds.h == 40.0F);
    assert(bounds.y + bounds.h == 500.0F - SpeechLayout::kGapAboveHead);
    assert(bounds.x >= 0.0F && bounds.x + bounds.w <= static_cast<float>(World::width));
    // Each line is centered in the block.
    const SDL_FPoint second = layout.lineOrigin(1);
    assert(second.x == bounds.x + (820.0F - 380.0F) * 0.5F && second.y == bounds.y + 20.0F);

    // A speaker name takes a line of its own above the text.
    assert(layout.update("Oi", "Guarda", kMono));
    layout.place({640.0F, 500.0F});
    assert(layout.lines().size() == 1 && layout.bounds().h == 40.0F && layout.bounds().w == 60.0F);
    assert(layout.speakerOrigin().y == layout.bounds().y && layout.lineOrigin(0).y
                                                                == layout.bounds().y + 20.0F);

    // New metrics (another pixel scale) re-wrap the same text.
    class Revised final : public TextMetrics {
    public:
        [[nodiscard]] float lineHeight(FontRole) const override { return 20.0F; }
        [[nodiscard]] float width(FontRole, const std::string_view text) const override {
            return 10.0F * static_cast<float>(text.size());
        }
        [[nodiscard]] std::uint32_t revision() const override { return 7; }
    };
    assert(layout.update("Oi", "Guarda", Revised{}));
}

} // namespace

int main() {
    testWrapsAtSpaces();
    testCountsCodePointsNotBytes();
    testExplicitNewlines();
    testSplitsWordsWiderThanTheLine();
    testUsesTheInjectedMeasure();
    testPlaceAboveStaysInside();
    testSpeechLayout();
}
