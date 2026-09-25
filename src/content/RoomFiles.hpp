#pragma once

#include "game/Room.hpp"
#include <filesystem>
#include <optional>

struct RoomLoadResult {
    std::optional<Room> room;
    std::string error;
};

namespace RoomFiles {
[[nodiscard]] RoomLoadResult load(const std::filesystem::path& path);
// Writes via a temporary file so a failed write preserves the previous room.
[[nodiscard]] bool save(const std::filesystem::path& path, const Room& room, std::string& error);
}
