#include "content/SpriteFields.hpp"

#include "content/TextFormat.hpp"
#include "game/Identifier.hpp"

#include <cmath>
#include <locale>
#include <ostream>
#include <sstream>

namespace {
bool number(const std::string& value, float& result) {
    std::istringstream input(value);
    input.imbue(std::locale::classic());
    if (!(input >> result) || !std::isfinite(result)) return false;
    input >> std::ws;
    return input.eof();
}
}

bool SpriteFields::parse(SpriteClip& clip, const std::string_view field,
                         const std::string& value) {
    if (field == "file") { clip.file = value; return true; }
    if (field == "loop" || field == "pixelated" || field == "flip_x") {
        if (value != "0" && value != "1") return false;
        bool& target = field == "loop" ? clip.loop
                       : field == "pixelated" ? clip.pixelated
                                              : clip.flipX;
        target = value == "1";
        return true;
    }
    float parsed = 0.0F;
    if (!number(value, parsed)) return false;
    const auto integer = [&](int& target) {
        if (std::floor(parsed) != parsed || parsed < -8192 || parsed > 8192) return false;
        target = static_cast<int>(parsed);
        return true;
    };
    if (field == "frame_width") return integer(clip.frameWidth);
    if (field == "frame_height") return integer(clip.frameHeight);
    if (field == "frames") return integer(clip.frames);
    if (field == "columns") return integer(clip.columns);
    if (field == "first_frame") return integer(clip.firstFrame);
    if (field == "margin_x") return integer(clip.marginX);
    if (field == "margin_y") return integer(clip.marginY);
    if (field == "spacing_x") return integer(clip.spacingX);
    if (field == "spacing_y") return integer(clip.spacingY);
    if (field == "fps") { clip.fps = parsed; return true; }
    if (field == "display_width") { clip.displayWidth = parsed; return true; }
    if (field == "display_height") { clip.displayHeight = parsed; return true; }
    if (field == "offset_x") { clip.offsetX = parsed; return true; }
    if (field == "offset_y") { clip.offsetY = parsed; return true; }
    return false;
}

void SpriteFields::write(std::ostream& out, const std::string_view prefix, const SpriteClip& clip,
                         const ClipKeys keys) {
    const SpriteClip defaults;
    // ClipKeys::all writes the key unconditionally (the historical layout of objects and player).
    const auto put = [&](const std::string_view name, const auto& value, const bool isDefault) {
        if (keys == ClipKeys::all || !isDefault) out << prefix << name << " = " << value << '\n';
    };
    const auto real = [&](const std::string_view name, const float value, const float fallback) {
        put(name, TextFormat::number(value), value == fallback);
    };
    put("file", clip.file, clip.file == defaults.file);
    put("frame_width", clip.frameWidth, clip.frameWidth == defaults.frameWidth);
    put("frame_height", clip.frameHeight, clip.frameHeight == defaults.frameHeight);
    put("frames", clip.frames, clip.frames == defaults.frames);
    put("columns", clip.columns, clip.columns == defaults.columns);
    put("first_frame", clip.firstFrame, clip.firstFrame == defaults.firstFrame);
    put("margin_x", clip.marginX, clip.marginX == defaults.marginX);
    put("margin_y", clip.marginY, clip.marginY == defaults.marginY);
    put("spacing_x", clip.spacingX, clip.spacingX == defaults.spacingX);
    put("spacing_y", clip.spacingY, clip.spacingY == defaults.spacingY);
    real("fps", clip.fps, defaults.fps);
    put("loop", clip.loop ? 1 : 0, clip.loop == defaults.loop);
    put("pixelated", clip.pixelated ? 1 : 0, clip.pixelated == defaults.pixelated);
    // Written only when set, so files authored before flip_x existed save unchanged.
    if (clip.flipX) out << prefix << "flip_x = 1\n";
    real("display_width", clip.displayWidth, defaults.displayWidth);
    real("display_height", clip.displayHeight, defaults.displayHeight);
    real("offset_x", clip.offsetX, defaults.offsetX);
    real("offset_y", clip.offsetY, defaults.offsetY);
}

bool SpriteFields::parseNamed(AnimationSet& animations, const std::string_view key,
                              const std::string_view prefix, const std::string& value) {
    if (!key.starts_with(prefix)) return false;
    const auto rest = key.substr(prefix.size());
    const auto separator = rest.find('.');
    if (separator == std::string_view::npos) return false;
    const auto name = rest.substr(0, separator);
    if (!validIdentifier(name)) return false;
    const SpriteClip* current = findAnimation(animations, name);
    SpriteClip clip = current ? *current : SpriteClip{};
    if (!SpriteFields::parse(clip, rest.substr(separator + 1), value)) return false;
    animations[std::string(name)] = std::move(clip);
    return true;
}
