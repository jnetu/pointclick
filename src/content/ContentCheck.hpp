#pragma once

#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Offline validation of a rooms folder, for `pointclick --check` and CI: every <id>.room with
// its .rules and .dlg (as the game loads them, RoomLibrary::loadBundle), every change_room
// target (RoomLibrary::transitionsError), the shared player.actor, the start room and companion
// files with no room. Desktop only: it lists the folder with std::filesystem.
// Art the game can run without (content/ArtCheck: PNG missing or unreadable, cut outside the
// image, font missing) comes as warnings: reported, counted, and never a failure.
struct ContentProblem {
    std::string file;    // "first.rules"; the folder itself when it cannot be read
    std::string message; // "Linha 4: Use set_solid <objeto> <0|1>"
};

struct ContentCheckReport {
    std::size_t roomsChecked = 0;
    std::vector<ContentProblem> problems; // sorted by file name
    // Each distinct warning once, under the first room that shows it ("first.room",
    // "sprites/door.png: PNG ausente"); fonts under "assets".
    std::vector<ContentProblem> warnings;

    [[nodiscard]] bool ok() const { return problems.empty(); } // warnings never fail the check
};

// fontFiles: the fonts the game opens, relative to <assets>/fonts (FontLibrary::files).
[[nodiscard]] ContentCheckReport checkContent(const std::filesystem::path& roomsDirectory,
                                              std::span<const std::string_view> fontFiles = {});
