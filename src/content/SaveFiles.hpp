#pragma once

#include "content/RoomLibrary.hpp"
#include "game/SavedGame.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

struct SaveLoadResult {
    std::optional<SavedGame> save;
    std::string error; // empty without a save = there is no save file
};

// What SaveFiles::resume does with a save it refuses.
enum class RefusedSave : std::uint8_t {
    quarantine, // renamed to <file>.bad, so the autosave of the new game never overwrites it
    keep,       // left exactly as it is (--no-save: nothing on disk changes)
};

// A save the boot can resume: the progress plus its room, loaded and cross-checked.
struct ResumeResult {
    std::optional<SavedGame> save;
    std::optional<RoomBundle> bundle;
    std::string error;   // why a present save was refused (see RefusedSave for what became of it)
    std::string warning; // set when the saved room did not load and the start room took its place
    // The refused file really became <file>.bad. False for RefusedSave::keep, when nothing was
    // refused, and when the rename failed (the reason is then appended to `error`).
    bool setAside = false;
};

// The match progress file (SavedGame), in the `key = value` style of .room:
//   version = 3                          first key; older versions stay readable
//   room = gallery                       <id>.room under the rooms folder
//   feet = 220, 460
//   facing = left|right|up|down          since version 2, optional (default down)
//   flag.<id> = <integer != 0>
//   item = <id>                          repeatable, inventory order
//   object.<room>.<object>.visible = 0|1 and .solid = 0|1 (each optional)
//   object.<room>.<object>.animation = <clip>   since version 3, optional (set_animation)
//   dialogue = <room>/<node>/<choice>    repeatable, `once` choices already taken
// '#' starts a comment; unknown or repeated keys and invalid IDs are errors with the line.
// A new key bumps currentVersion, so an older build refuses the file by version instead of a
// confusing key error; older versions keep parsing with new fields at their defaults.
// Only stable IDs are stored: a clip (or object) the room no longer has is harmless on load.
// History: 1 = room, feet, flags, items, objects, dialogue; 2 = + facing;
//          3 = + object.<room>.<object>.animation.
namespace SaveFiles {
inline constexpr int currentVersion = 3;
inline constexpr std::string_view slotName = "save.slot";
// SDL_GetPrefPath(organization, application): the per-user writable folder on every platform.
inline constexpr const char* organization = "pointclick";
inline constexpr const char* application = "pointclick";

[[nodiscard]] SaveLoadResult parse(std::string_view text);
// Deterministic: the same SavedGame always gives the same bytes (written with currentVersion).
[[nodiscard]] std::string serialize(const SavedGame& save);

// A missing file is no save and no error.
[[nodiscard]] SaveLoadResult load(const std::filesystem::path& path);
// Atomic (<path>.tmp then rename): a failed write keeps the previous save intact.
[[nodiscard]] bool save(const std::filesystem::path& path, const SavedGame& game,
                        std::string& error);
// Moves a refused save to <path>.bad (replacing an older one), so a new game never overwrites it.
bool quarantine(const std::filesystem::path& path, std::string& error);

// <SDL_GetPrefPath>/save.slot (SDL creates the folder). Empty, with the error, when the
// platform has no writable folder; the game then runs without saves.
[[nodiscard]] std::filesystem::path defaultSlot(std::string& error);

// Loads the save and its room. Missing: nothing, no error. Refused (corrupt, unknown version):
// reason in error, and the file is renamed to .bad or left alone, as `refused` says; this is the
// only write resume ever does. A saved room that no longer loads costs no progress: it resumes
// in fallbackRoom at playerStart (reason in warning); refused only if that fails too.
[[nodiscard]] ResumeResult resume(const std::filesystem::path& path, const RoomLibrary& library,
                                  RefusedSave refused,
                                  std::string_view fallbackRoom = RoomLibrary::startRoomFile);
} // namespace SaveFiles
