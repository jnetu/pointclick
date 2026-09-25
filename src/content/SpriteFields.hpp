#pragma once

#include "game/SpriteClip.hpp"

#include <iosfwd>
#include <string>
#include <string_view>

// Serialization of one clip; room and object schemas decide the key prefix.
namespace SpriteFields {
[[nodiscard]] bool parse(SpriteClip& clip, std::string_view field,
                         const std::string& value);
void write(std::ostream& out, std::string_view prefix, const SpriteClip& clip);
}
