#pragma once

#include "game/Dialogue.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

struct Room;

struct DialogueLoadResult {
    std::optional<RoomDialogues> dialogues;
    std::string error;
};

// <room>.dlg: the room's dialogue nodes, beside <room>.room and <room>.rules.
//
//   [node guarda]                      # a node; start_dialogue guarda opens it
//   say "Alto la!"                     # effects run when the node opens
//   [choice chave "Tenho a chave."]    # an option of the node above
//   has_item chave                     # conditions first: listed only when all hold
//   once                               # hidden after being chosen once
//   take_item chave                    # effects run when chosen
//   goto liberado                      # last line, optional: next node (none = end)
namespace DialogueFiles {
[[nodiscard]] std::filesystem::path companionPath(const std::filesystem::path& roomPath);

[[nodiscard]] DialogueLoadResult parse(std::string_view content, const Room* room = nullptr);
[[nodiscard]] DialogueLoadResult load(const std::filesystem::path& path,
                                      const Room* room = nullptr);
// Loads <room>.dlg when present; returns empty RoomDialogues without error when absent.
[[nodiscard]] DialogueLoadResult loadOptionalForRoom(const std::filesystem::path& roomPath,
                                                     const Room* room = nullptr);

// Writes via a temporary file so a failed write preserves the previous .dlg file.
[[nodiscard]] bool save(const std::filesystem::path& path, const RoomDialogues& dialogues,
                        std::string& error, const Room* room = nullptr);
} // namespace DialogueFiles
