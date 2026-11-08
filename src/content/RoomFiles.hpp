#pragma once

#include "game/Room.hpp"
#include <filesystem>
#include <optional>

struct RoomLoadResult {
    std::optional<Room> room;
    std::string error;
};

namespace RoomFiles {
// inheritedPlayerArt (player.actor) seeds Room::playerAnimations; player.<clip>.* keys add or
// override clips. Without it the room declares every locomotion clip itself.
[[nodiscard]] RoomLoadResult load(const std::filesystem::path& path,
                                  const AnimationSet* inheritedPlayerArt = nullptr);
// Writes via a temporary file so a failed write preserves the previous room. Clips equal to
// inheritedPlayerArt are left out, so saving never copies the shared art into the room.
[[nodiscard]] bool save(const std::filesystem::path& path, const Room& room, std::string& error,
                        const AnimationSet* inheritedPlayerArt = nullptr);
}
