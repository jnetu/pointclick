#pragma once

#include "content/RoomFiles.hpp"
#include "game/Animation.hpp"
#include "game/Dialogue.hpp"
#include "game/Interaction.hpp"
#include "game/Room.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

// A room ready to apply: definition plus its optional .rules and .dlg companions, validated
// against each other (rule objects, start_dialogue targets).
struct RoomBundle {
    Room room;
    RoomInteractions interactions;
    RoomDialogues dialogues;
};

struct RoomBundleResult {
    std::optional<RoomBundle> bundle;
    std::string error;
    std::string errorFile; // the file the error is about ("first.rules"); empty on success
};

// The asset layout in one place:
//   <assets>/rooms/<id>.room|.rules|.dlg
//   <assets>/actors/player.actor   shared player art every room inherits
//   <assets>/sprites/              PNGs, relative paths in clips
//   <assets>/fonts/                TTF files of the game text (graphics/FontLibrary)
// The game and the tools load and save rooms through it, so both see the same inherited art.
class RoomLibrary {
public:
    // Where a new game starts, and where a save whose room no longer loads resumes.
    static constexpr std::string_view startRoomFile = "first.room";

    // roomsDirectory is <assets>/rooms. A missing player.actor is allowed (rooms then declare
    // their own player clips); an invalid one is an error.
    [[nodiscard]] bool open(std::filesystem::path roomsDirectory, std::string& error);

    [[nodiscard]] const std::filesystem::path& roomsDirectory() const { return roomsDirectory_; }
    [[nodiscard]] std::filesystem::path spritesDirectory() const;
    [[nodiscard]] std::filesystem::path fontsDirectory() const;
    [[nodiscard]] std::filesystem::path playerActorPath() const;
    [[nodiscard]] const AnimationSet& playerArt() const { return playerArt_; }

    // The file name is the room's identity: "<id>.room". Loading rejects a file whose `id`
    // differs from its name; saving writes the id of the target name.
    [[nodiscard]] static std::string roomIdFor(std::string_view filename);
    [[nodiscard]] RoomLoadResult loadRoom(std::string_view filename) const;
    [[nodiscard]] RoomBundleResult loadBundle(std::string_view filename) const;
    // Every change_room (rules and dialogue) names a room that loads and a spawn it has.
    // Content check only (reads the target rooms); at runtime a bad target just refuses the
    // transition. Empty means valid.
    [[nodiscard]] std::string transitionsError(const RoomBundle& bundle) const;
    // Leaves out clips equal to the shared player art.
    [[nodiscard]] bool saveRoom(std::string_view filename, const Room& room,
                                std::string& error) const;

private:
    std::filesystem::path roomsDirectory_;
    AnimationSet playerArt_;
};
