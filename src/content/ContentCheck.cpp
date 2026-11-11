#include "content/ContentCheck.hpp"

#include "content/RoomLibrary.hpp"
#include "content/TextFormat.hpp"

#include <algorithm>
#include <set>
#include <system_error>

ContentCheckReport checkContent(const std::filesystem::path& roomsDirectory) {
    ContentCheckReport report;
    const auto problem = [&report](std::string file, std::string message) {
        report.problems.push_back({std::move(file), std::move(message)});
    };

    RoomLibrary library;
    if (std::string error; !library.open(roomsDirectory, error)) {
        // Already "player.actor: Linha N: ..."; without the actor no room loads as in the game.
        problem("player.actor", error.starts_with("player.actor: ") ? error.substr(14) : error);
        return report;
    }

    std::error_code ec;
    std::set<std::string> rooms;     // file names, sorted for a stable report
    std::set<std::string> companions; // .rules / .dlg
    for (std::filesystem::directory_iterator it(roomsDirectory, ec), end; !ec && it != end;
         it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        const std::filesystem::path& path = it->path();
        const std::string name = TextFormat::pathToUtf8(path.filename());
        if (path.extension() == ".room") rooms.insert(name);
        else if (path.extension() == ".rules" || path.extension() == ".dlg") companions.insert(name);
    }
    if (ec) {
        problem(TextFormat::pathToUtf8(roomsDirectory), "Nao foi possivel listar a pasta: "
                                                                + ec.message());
        return report;
    }

    if (!rooms.contains(std::string(RoomLibrary::startRoomFile))) {
        problem(std::string(RoomLibrary::startRoomFile), "Sala inicial ausente");
    }
    for (const std::string& companion : companions) {
        const std::string room = TextFormat::pathToUtf8(
                std::filesystem::path(companion).replace_extension(".room"));
        if (!rooms.contains(room)) problem(companion, "Sem " + room + " correspondente");
    }
    for (const std::string& room : rooms) {
        ++report.roomsChecked;
        RoomBundleResult loaded = library.loadBundle(room);
        if (!loaded.bundle) {
            problem(loaded.errorFile.empty() ? room : loaded.errorFile, loaded.error);
            continue;
        }
        if (std::string error = library.transitionsError(*loaded.bundle); !error.empty()) {
            // change_room may come from the .rules or from a dialogue of the .dlg.
            const std::string stem = RoomLibrary::roomIdFor(room);
            problem(stem + ".rules/.dlg", error);
        }
    }
    std::stable_sort(report.problems.begin(), report.problems.end(),
                     [](const ContentProblem& a, const ContentProblem& b) { return a.file < b.file; });
    return report;
}
