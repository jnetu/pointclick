#pragma once

#include "game/Room.hpp"

namespace RoomPresets {
// Defaults used for keys omitted from .room files. Art references live here,
// outside the generic room data type.
[[nodiscard]] Room defaults();
[[nodiscard]] Room firstRoom();
}
