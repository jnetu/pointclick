#include "content/SpriteFields.hpp"

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
    if (field == "loop" || field == "pixelated") {
        if (value != "0" && value != "1") return false;
        (field == "loop" ? clip.loop : clip.pixelated) = value == "1";
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

void SpriteFields::write(std::ostream& out, const std::string_view prefix,
                         const SpriteClip& clip) {
    out << prefix << "file = " << clip.file << '\n';
    out << prefix << "frame_width = " << clip.frameWidth << '\n';
    out << prefix << "frame_height = " << clip.frameHeight << '\n';
    out << prefix << "frames = " << clip.frames << '\n';
    out << prefix << "columns = " << clip.columns << '\n';
    out << prefix << "first_frame = " << clip.firstFrame << '\n';
    out << prefix << "margin_x = " << clip.marginX << '\n';
    out << prefix << "margin_y = " << clip.marginY << '\n';
    out << prefix << "spacing_x = " << clip.spacingX << '\n';
    out << prefix << "spacing_y = " << clip.spacingY << '\n';
    out << prefix << "fps = " << clip.fps << '\n';
    out << prefix << "loop = " << (clip.loop ? 1 : 0) << '\n';
    out << prefix << "pixelated = " << (clip.pixelated ? 1 : 0) << '\n';
    out << prefix << "display_width = " << clip.displayWidth << '\n';
    out << prefix << "display_height = " << clip.displayHeight << '\n';
    out << prefix << "offset_x = " << clip.offsetX << '\n';
    out << prefix << "offset_y = " << clip.offsetY << '\n';
}
