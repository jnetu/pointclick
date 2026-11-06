#pragma once

#include "game/Animation.hpp"
#include "game/SpriteClip.hpp"

#include <iosfwd>
#include <string>
#include <string_view>

// Serialization of one clip; room and object schemas decide the key prefix.
namespace SpriteFields {
[[nodiscard]] bool parse(SpriteClip& clip, std::string_view field,
                         const std::string& value);
void write(std::ostream& out, std::string_view prefix, const SpriteClip& clip);
// "<prefix><clip>.<field> = value" into a named set; an existing clip is updated field by field,
// so a room can override one field of an inherited clip.
[[nodiscard]] bool parseNamed(AnimationSet& animations, std::string_view key,
                              std::string_view prefix, const std::string& value);
}
