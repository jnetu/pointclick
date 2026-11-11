// F1: command-line options (parsed without SDL) and the --check content validation.
#include "content/ContentCheck.hpp"
#include "core/CommandLine.hpp"

#include "support/TempDirectory.hpp"

#include <doctest/doctest.h>
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

TEST_CASE("testParseCommandLine") {
    const CommandLineResult none = parse({});
    REQUIRE((none.options && none.error.empty()));
    REQUIRE((none.options->roomsDirectory.empty() && none.options->resumesSave()));
    REQUIRE((!none.options->newGame && !none.options->noSave && !none.options->check));

    const CommandLineResult all =
            parse({"--rooms", "assets/rooms", "--new-game", "--no-save", "--room", "gallery", "door"});
    REQUIRE(all.options);
    REQUIRE(all.options->roomsDirectory == std::filesystem::path("assets/rooms"));
    REQUIRE((all.options->newGame && all.options->noSave && !all.options->resumesSave()));
    REQUIRE((all.options->startRoom == "gallery" && all.options->startSpawn == "door"));

    // The spawn is optional, and an option after the room is not taken as one.
    const CommandLineResult room = parse({"--room", "gallery", "--no-save"});
    REQUIRE((room.options && room.options->startRoom == "gallery"));
    REQUIRE((room.options->startSpawn.empty() && room.options->noSave));
    REQUIRE(!room.options->resumesSave()); // --room starts a new game

    // UTF-8 folder names survive the conversion to a path.
    const CommandLineResult utf8 = parse({"--rooms", "salas-a\xC3\xA7\xC3\xA3o"});
    REQUIRE((utf8.options
             && utf8.options->roomsDirectory == std::filesystem::path(u8"salas-ação")));

    const CommandLineResult check = parse({"--check", "--rooms", "x"});
    REQUIRE((check.options && check.options->check));

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
        REQUIRE((!result.options && result.error == entry.error));
    }
    REQUIRE(!commandLineUsage().empty());
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

TEST_CASE("testCheckShippedAssets") {
    const auto rooms = std::filesystem::path(POINTCLICK_SOURCE_DIR) / "assets/rooms";
    const ContentCheckReport report = checkContent(rooms);
    REQUIRE(report.ok());
    REQUIRE(report.roomsChecked == 2);
    REQUIRE(report.warnings.empty()); // every PNG exists and every cut fits it

    // F8: a font the game would open and the folder lacks is a warning, never an error.
    const std::string_view fonts[]{"lora/Lora-Regular.ttf", "nope/Missing.ttf", "nope/Missing.ttf"};
    const ContentCheckReport withFonts = checkContent(rooms, fonts);
    REQUIRE((withFonts.ok() && withFonts.warnings.size() == 1));
    REQUIRE(withFonts.warnings[0].file == "assets");
    REQUIRE(withFonts.warnings[0].message == "fonts/nope/Missing.ttf: Fonte ausente");
}

// F8: missing art is reported and counted, and the folder still passes (exit code 0).
TEST_CASE("testCheckWarnsAboutMissingArt") {
    const auto rooms = std::filesystem::path(POINTCLICK_SOURCE_DIR)
                       / "tests/fixtures/missing_art/rooms";
    const ContentCheckReport report = checkContent(rooms);
    REQUIRE((report.ok() && report.problems.empty()));
    REQUIRE(report.roomsChecked == 1);
    // One warning per file, however many clips use it.
    REQUIRE(report.warnings.size() == 2);
    REQUIRE(report.warnings[0].file == "first.room");
    REQUIRE(report.warnings[0].message == "sprites/player.png: PNG ausente");
    REQUIRE(report.warnings[1].message == "sprites/ghost.png: PNG ausente");
}

TEST_CASE("testCheckReportsErrorsByFileAndLine") {
    const TempDirectory testDirectory("pointclick-cli-tests");
    const std::filesystem::path& directory = testDirectory.path();
    const auto source = std::filesystem::path(POINTCLICK_SOURCE_DIR) / "assets";
    std::filesystem::copy(source / "rooms", directory / "rooms");
    std::filesystem::copy(source / "actors", directory / "actors");
    const auto rooms = directory / "rooms";
    REQUIRE(checkContent(rooms).ok()); // the copy is as good as the original

    append(rooms / "first.rules", "\n[interact blue_box]\nset_solid red_box 2\n"); // lines 34-35
    append(rooms / "gallery.rules", "\n[interact plant]\nchange_room nowhere\n");
    std::ofstream(rooms / "lost.dlg", std::ios::binary) << "[node a]\nsay \"oi\"\n";

    const ContentCheckReport report = checkContent(rooms);
    REQUIRE(!report.ok());
    REQUIRE(report.roomsChecked == 2);
    REQUIRE(hasProblem(report, "first.rules", "Linha 35: Use set_solid <objeto> <0|1>"));
    REQUIRE(hasProblem(report, "gallery.rules/.dlg",
                       "change_room nowhere: Nao foi possivel abrir nowhere.room"));
    REQUIRE(hasProblem(report, "lost.dlg", "Sem lost.room correspondente"));
    REQUIRE(report.problems.size() == 3);

    // No start room and an unreadable player.actor are reported too.
    std::filesystem::remove(rooms / "first.room");
    REQUIRE(hasProblem(checkContent(rooms), "first.room", "Sala inicial ausente"));
    std::ofstream(directory / "actors" / "player.actor", std::ios::binary) << "idle.fps = x\n";
    const ContentCheckReport actor = checkContent(rooms);
    REQUIRE((actor.problems.size() == 1 && actor.problems[0].file == "player.actor"));
    REQUIRE(actor.problems[0].message == "Linha 1: Chave ou valor invalido: idle.fps");

    // A folder that does not exist fails instead of passing with zero rooms.
    std::filesystem::remove_all(directory / "actors");
    REQUIRE(!checkContent(directory / "nao_existe").ok());
}
} // namespace
