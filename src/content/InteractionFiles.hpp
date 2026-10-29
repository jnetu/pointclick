#pragma once

#include "game/Interaction.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

struct Room;

struct InteractionLoadResult {
    std::optional<RoomInteractions> interactions;
    std::string error;
};

namespace InteractionFiles {
[[nodiscard]] std::filesystem::path companionPath(const std::filesystem::path& roomPath);

[[nodiscard]] InteractionLoadResult parse(std::string_view content, const Room* room = nullptr);
[[nodiscard]] InteractionLoadResult load(const std::filesystem::path& path,
                                         const Room* room = nullptr);

// Loads <room>.rules when present; returns empty RoomInteractions without error when absent.
[[nodiscard]] InteractionLoadResult loadOptionalForRoom(const std::filesystem::path& roomPath,
                                                        const Room* room = nullptr);

// Writes via a temporary file so a failed write preserves the previous .rules file.
[[nodiscard]] bool save(const std::filesystem::path& path,
                        const RoomInteractions& interactions,
                        std::string& error,
                        const Room* room = nullptr);
} // namespace InteractionFiles
