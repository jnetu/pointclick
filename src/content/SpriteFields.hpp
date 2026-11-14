#pragma once

#include "game/Animation.hpp"
#include "game/SpriteClip.hpp"

#include <cstdint>
#include <iosfwd>
#include <string>
#include <string_view>

// Serialization of one clip; room and object schemas decide the key prefix.
namespace SpriteFields {
[[nodiscard]] bool parse(SpriteClip& clip, std::string_view field,
                         const std::string& value);
// all: every key (objects and player, as files have always been written); changed: only the
// keys that differ from a default SpriteClip (layers, whose blocks stay as short as authored).
enum class ClipKeys : std::uint8_t { all, changed };
void write(std::ostream& out, std::string_view prefix, const SpriteClip& clip,
           ClipKeys keys = ClipKeys::all);
// "<prefix><clip>.<field> = value" into a named set; an existing clip is updated field by field,
// so a room can override one field of an inherited clip.
[[nodiscard]] bool parseNamed(AnimationSet& animations, std::string_view key,
                              std::string_view prefix, const std::string& value);
}
