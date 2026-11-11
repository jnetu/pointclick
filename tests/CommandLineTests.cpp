// F1: command-line options (parsed without SDL) and the --check content validation.
#include "content/ContentCheck.hpp"
#include "core/CommandLine.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

namespace {
CommandLineResult parse(const std::initializer_list<std::string_view> args) {
    const std::vector<std::string_view> list(args);
    return parseCommandLine(list);
}

void testParseCommandLine() {
    const CommandLineResult none = parse({});
    assert(none.options && none.error.empty());
    assert(none.options->roomsDirectory.empty() && none.options->resumesSave());
    assert(!none.options->newGame && !none.options->noSave && !none.options->check);

    const CommandLineResult all =
            parse({"--rooms", "assets/rooms", "--new-game", "--no-save", "--room", "gallery", "door"});
    assert(all.options);
    assert(all.options->roomsDirectory == std::filesystem::path("assets/rooms"));
    assert(all.options->newGame && all.options->noSave && !all.options->resumesSave());
    assert(all.options->startRoom == "gallery" && all.options->startSpawn == "door");

    // The spawn is optional, and an option after the room is not taken as one.
    const CommandLineResult room = parse({"--room", "gallery", "--no-save"});
    assert(room.options && room.options->startRoom == "gallery");
    assert(room.options->startSpawn.empty() && room.options->noSave);
    assert(!room.options->resumesSave()); // --room starts a new game

    // UTF-8 folder names survive the conversion to a path.
    const CommandLineResult utf8 = parse({"--rooms", "salas-a\xC3\xA7\xC3\xA3o"});
    assert(utf8.options
           && utf8.options->roomsDirectory == std::filesystem::path(u8"salas-ação"));

    const CommandLineResult check = parse({"--check", "--rooms", "x"});
    assert(check.options && check.options->check);

    const struct {
        std::initializer_list<std::string_view> args;
        const char* error;
    } invalid[]{
        {{"--rooms"}, "--rooms precisa de uma pasta"},
        {{"--room"}, "--room precisa do id de uma sala"},
        {{"--room", "--new-game"}, "--room precisa do id de uma sala"},
        {{"--room", "sala/1"}, "Id de sala invalido: sala/1"},
        {{"--room", "a", "b c"}, "Id de spawn invalido: b c"},
        {{"--room", "a", "--room", "b"}, "Opcao repetida: --room"},
        {{"--no-save", "--no-save"}, "Opcao repetida: --no-save"},
        {{"--rooms", "a", "--rooms", "b"}, "Opcao repetida: --rooms"},
        {{"--check", "--new-game"}, "--check aceita apenas --rooms"},
        {{"--fast"}, "Opcao desconhecida: --fast"},
        {{"assets/rooms"}, "Opcao desconhecida: assets/rooms"},
    };
    for (const auto& entry : invalid) {
        const std::vector<std::string_view> list(entry.args);
        const CommandLineResult result = parseCommandLine(list);
        assert(!result.options && result.error == entry.error);
    }
    assert(!commandLineUsage().empty());
}

void append(const std::filesystem::path& file, const std::string_view text) {
    std::ofstream(file, std::ios::binary | std::ios::app) << text;
}

bool hasProblem(const ContentCheckReport& report, const std::string_view file,
                const std::string_view message) {
    for (const ContentProblem& problem : report.problems) {
        if (problem.file == file && problem.message == message) return true;
    }
    return false;
}

void testCheckShippedAssets() {
    const auto rooms = std::filesystem::path(POINTCLICK_SOURCE_DIR) / "assets/rooms";
    const ContentCheckReport report = checkContent(rooms);
    assert(report.ok());
    assert(report.roomsChecked == 2);
}

void testCheckReportsErrorsByFileAndLine(const std::filesystem::path& directory) {
    const auto source = std::filesystem::path(POINTCLICK_SOURCE_DIR) / "assets";
    std::filesystem::copy(source / "rooms", directory / "rooms");
    std::filesystem::copy(source / "actors", directory / "actors");
    const auto rooms = directory / "rooms";
    assert(checkContent(rooms).ok()); // the copy is as good as the original

    append(rooms / "first.rules", "\n[interact blue_box]\nset_solid red_box 2\n"); // lines 34-35
    append(rooms / "gallery.rules", "\n[interact plant]\nchange_room nowhere\n");
    std::ofstream(rooms / "lost.dlg", std::ios::binary) << "[node a]\nsay \"oi\"\n";

    const ContentCheckReport report = checkContent(rooms);
    assert(!report.ok());
    assert(report.roomsChecked == 2);
    assert(hasProblem(report, "first.rules", "Linha 35: Use set_solid <objeto> <0|1>"));
    assert(hasProblem(report, "gallery.rules/.dlg",
                      "change_room nowhere: Nao foi possivel abrir nowhere.room"));
    assert(hasProblem(report, "lost.dlg", "Sem lost.room correspondente"));
    assert(report.problems.size() == 3);

    // No start room and an unreadable player.actor are reported too.
    std::filesystem::remove(rooms / "first.room");
    assert(hasProblem(checkContent(rooms), "first.room", "Sala inicial ausente"));
    std::ofstream(directory / "actors" / "player.actor", std::ios::binary) << "idle.fps = x\n";
    const ContentCheckReport actor = checkContent(rooms);
    assert(actor.problems.size() == 1 && actor.problems[0].file == "player.actor");
    assert(actor.problems[0].message == "Linha 1: Chave ou valor invalido: idle.fps");

    // A folder that does not exist fails instead of passing with zero rooms.
    std::filesystem::remove_all(directory / "actors");
    assert(!checkContent(directory / "nao_existe").ok());
}
} // namespace

int main() {
    testParseCommandLine();
    testCheckShippedAssets();

    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("pointclick-cli-tests-" + std::to_string(suffix));
    std::filesystem::create_directory(directory);
    testCheckReportsErrorsByFileAndLine(directory);
    std::filesystem::remove_all(directory);
}
