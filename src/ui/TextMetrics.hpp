#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

// What a piece of game text is, not which file draws it. graphics/FontLibrary maps each role to a
// font file and size in one place; layout code (ui/) only asks for measures by role.
enum class FontRole : std::uint8_t {
    dialogue, // spoken lines and remarks (Atkinson Hyperlegible)
    choice,   // dialogue choice menu (Atkinson Hyperlegible)
    speaker,  // name of who is speaking (Lora)
    title,    // titles and headings (Lora)
};
inline constexpr std::size_t kFontRoleCount = 4;

// Text measures in logical viewport units (World::width x World::height), independent of the
// window's pixel density. Injected into ui/ so layout never includes graphics/.
class TextMetrics {
public:
    virtual ~TextMetrics() = default;
    // Distance between baselines of consecutive lines.
    [[nodiscard]] virtual float lineHeight(FontRole role) const = 0;
    // Advance width of one line of UTF-8 text (no line breaks inside).
    [[nodiscard]] virtual float width(FontRole role, std::string_view text) const = 0;
    // Changes whenever measures change (a different pixel scale, fonts loaded or lost), so
    // cached layouts know when to wrap again.
    [[nodiscard]] virtual std::uint32_t revision() const { return 0; }
};

// Every code point has the same width: the SDL debug font fallback and headless tests.
class MonospaceTextMetrics final : public TextMetrics {
public:
    constexpr MonospaceTextMetrics(const float glyphWidth, const float lineHeightValue)
        : glyphWidth_(glyphWidth), lineHeight_(lineHeightValue) {}

    [[nodiscard]] float lineHeight(FontRole) const override { return lineHeight_; }
    [[nodiscard]] float width(FontRole, const std::string_view text) const override {
        std::size_t codePoints = 0;
        for (const char ch : text) {
            if ((static_cast<unsigned char>(ch) & 0xC0U) != 0x80U) ++codePoints;
        }
        return glyphWidth_ * static_cast<float>(codePoints);
    }

private:
    float glyphWidth_;
    float lineHeight_;
};
