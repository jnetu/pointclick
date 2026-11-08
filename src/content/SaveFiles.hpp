#pragma once

#include "content/RoomLibrary.hpp"
#include "game/SavedGame.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

struct SaveLoadResult {
    std::optional<SavedGame> save;
    std::string error; // empty without a save = there is no save file
};

// A save the boot can resume: the progress plus its room, loaded and cross-checked.
struct ResumeResult {
    std::optional<SavedGame> save;
    std::optional<RoomBundle> bundle;
    std::string error;   // why a present save was refused (it was renamed to <file>.bad)
    std::string warning; // set when the saved room did not load and the start room took its place
};

// The match progress file (SavedGame), in the `key = value` style of .room:
//   version = 2                          first key; older versions stay readable
//   room = gallery                       <id>.room under the rooms folder
//   feet = 220, 460
//   facing = left|right|up|down          since version 2, optional (default down)
//   flag.<id> = <integer != 0>
//   item = <id>                          repeatable, inventory order
//   object.<room>.<object>.visible = 0|1 and .solid = 0|1 (each optional)
//   dialogue = <room>/<node>/<choice>    repeatable, `once` choices already taken
// '#' starts a comment; unknown keys, repeated keys and invalid IDs are errors with the line.
// Any new key bumps currentVersion (a reader rejects keys it does not know, so an older build
// must refuse the file by its version, not by a confusing key error); every older version
// keeps parsing, with the new fields at their defaults.
// Version history: 1 = room, feet, flags, items, objects, dialogue; 2 = + facing.
namespace SaveFiles {
inline constexpr int currentVersion = 2;
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

// Loads the save at path and the room it names. Missing: nothing, no error. Refused (corrupt,
// unknown version): renamed to .bad and the reason in error.
// The progress does not depend on the room, so a saved room that no longer loads (removed or
// renamed by an update, broken file) does not cost it: the save resumes in fallbackRoom at its
// playerStart, with the reason in warning, and the file stays (the next autosave rewrites it).
// Only when fallbackRoom fails too is the save refused.
[[nodiscard]] ResumeResult resume(const std::filesystem::path& path, const RoomLibrary& library,
                                  std::string_view fallbackRoom = RoomLibrary::startRoomFile);
} // namespace SaveFiles
