#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

// Offline validation of a rooms folder, for `pointclick --check` and CI: every <id>.room with
// its .rules and .dlg (as the game loads them, RoomLibrary::loadBundle), every change_room
// target (RoomLibrary::transitionsError), the shared player.actor, the start room and companion
// files with no room. Desktop only: it lists the folder with std::filesystem.
struct ContentProblem {
    std::string file;    // "first.rules"; the folder itself when it cannot be read
    std::string message; // "Linha 4: Use set_solid <objeto> <0|1>"
};

struct ContentCheckReport {
    std::size_t roomsChecked = 0;
    std::vector<ContentProblem> problems; // sorted by file name

    [[nodiscard]] bool ok() const { return problems.empty(); }
};

[[nodiscard]] ContentCheckReport checkContent(const std::filesystem::path& roomsDirectory);
