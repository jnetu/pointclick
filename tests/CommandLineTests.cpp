// F1: command-line options (parsed without SDL) and the --check content validation.
// F2: --exec / --exec-file, run by debug/CommandScript in builds with the tools.
#include "content/ContentCheck.hpp"
#include "content/TextFormat.hpp"
#include "core/CommandLine.hpp"
#if POINTCLICK_TOOLS
#include "debug/CommandScript.hpp"
#endif

#include "support/TempDirectory.hpp"

#include <doctest/doctest.h>
#include <algorithm>
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

// How many <id>.room files the folder has: what checkContent has to report as checked.
std::size_t roomFileCount(const std::filesystem::path& directory) {
    std::size_t count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.path().extension() == ".room") ++count;
    }
    return count;
}

TEST_CASE("testCheckShippedAssets") {
    const auto rooms = std::filesystem::path(POINTCLICK_SOURCE_DIR) / "assets/rooms";
    const ContentCheckReport report = checkContent(rooms);
    REQUIRE(report.ok());
    REQUIRE(report.roomsChecked == roomFileCount(rooms)); // the playgrounds are checked too
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

    // The bad effect lands on the third appended line (a blank, the trigger, then the effect).
    const std::size_t badLine
            = static_cast<std::size_t>(std::ranges::count(
                      TextFormat::readFile(rooms / "first.rules").text.value_or(""), '\n'))
              + 3;
    append(rooms / "first.rules", "\n[interact blue_box]\nset_solid red_box 2\n");
    append(rooms / "gallery.rules", "\n[interact plant]\nchange_room nowhere\n");
    std::ofstream(rooms / "lost.dlg", std::ios::binary) << "[node a]\nsay \"oi\"\n";

    const ContentCheckReport report = checkContent(rooms);
    REQUIRE(!report.ok());
    REQUIRE(report.roomsChecked == roomFileCount(rooms));
    REQUIRE(hasProblem(report, "first.rules",
                       "Linha " + std::to_string(badLine) + ": Use set_solid <objeto> <0|1>"));
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

#if POINTCLICK_TOOLS
// F2: --exec / --exec-file keep their order and only go with --rooms and --room.
TEST_CASE("testParseExecOptions") {
    const CommandLineResult exec = parse({"--exec", "give brass_key", "--exec-file", "a.txt",
                                          "--exec", "state", "--room", "gallery", "--no-save"});
    REQUIRE((exec.options && exec.options->runsCommands()));
    const std::vector<ExecStep>& steps = exec.options->exec;
    REQUIRE(steps.size() == 3);
    REQUIRE((steps[0].command == "give brass_key" && steps[0].file.empty()));
    REQUIRE((steps[1].command.empty() && steps[1].file == std::filesystem::path("a.txt")));
    REQUIRE((steps[2].command == "state" && exec.options->startRoom == "gallery"));
    REQUIRE(!parse({}).options->runsCommands());
    REQUIRE(commandLineUsage().find("--exec-file <arquivo>") != std::string_view::npos);

    const struct {
        std::initializer_list<std::string_view> args;
        const char* error;
    } invalid[]{
        {{"--exec"}, "--exec precisa de um comando"},
        {{"--exec", ""}, "--exec precisa de um comando"},
        {{"--exec-file"}, "--exec-file precisa de um arquivo"},
        {{"--check", "--exec", "state"}, "--check aceita apenas --rooms"},
    };
    for (const auto& entry : invalid) {
        const std::vector<std::string_view> list(entry.args);
        const CommandLineResult result = parseCommandLine(list);
        REQUIRE((!result.options && result.error == entry.error));
    }
}

struct ExecRun {
    int exitCode = -1;
    std::vector<std::string> lines;

    [[nodiscard]] bool printed(const std::string_view line) const {
        return std::find(lines.begin(), lines.end(), line) != lines.end();
    }
};

ExecRun exec(const std::filesystem::path& rooms, const std::vector<CommandSource>& sources,
             const std::string_view startRoom = {}) {
    ExecRun run;
    run.exitCode = runCommandScript(
            rooms, startRoom, {}, sources,
            [&run](const std::string_view line) { run.lines.emplace_back(line); });
    return run;
}

std::filesystem::path shippedRooms() {
    return std::filesystem::path(POINTCLICK_SOURCE_DIR) / "assets/rooms";
}

CommandSource line(std::string command) {
    return {std::move(command), {}};
}
CommandSource file(std::filesystem::path path) {
    return {{}, std::move(path)};
}

// Regression: the shipped [use_item brass_key red_box] can be played without an inventory UI.
TEST_CASE("testExecUsesBrassKeyOnShippedRedBox") {
    const ExecRun run = exec(
            shippedRooms(), {line("give brass_key"), line("use brass_key red_box"), line("state")});
    REQUIRE(run.exitCode == 0);
    const std::vector<std::string> expected{
        "sala: first",
        "> give brass_key",
        "Item recebido: brass_key",
        "> use brass_key red_box",
        "Usando brass_key em red_box",
        "fala: A chave destravou a caixa vermelha! O caminho está livre.",
        "> state",
        "sala: first",
        "flags: red_box_unlocked=1",
        "itens: -",
        "objetos: first.red_box solid=0",
        "escolhas: -",
        "3 comando(s), 0 falha(s)",
    };
    REQUIRE(run.lines == expected);
}

// Every command runs even after a failure; any failure makes the exit code 1.
TEST_CASE("testExecReportsFailures") {
    const ExecRun run = exec(shippedRooms(), {line("use brass_key red_box"), line("nope"),
                                              line("give brass_key"), line("goto gallery")});
    REQUIRE(run.exitCode == 1);
    REQUIRE(run.printed("erro: Item fora do inventario: brass_key (give brass_key)"));
    REQUIRE(run.printed("erro: Comando desconhecido. Digite help."));
    REQUIRE(run.printed("Item recebido: brass_key"));
    REQUIRE(run.printed("sala: gallery"));
    REQUIRE(run.lines.back() == "4 comando(s), 2 falha(s)");

    // A start room that does not load: nothing runs.
    const ExecRun noRoom = exec(shippedRooms(), {line("state")}, "nowhere");
    REQUIRE((noRoom.exitCode == 1 && noRoom.lines.size() == 1));
    REQUIRE(noRoom.lines[0].starts_with("erro: nowhere.room: "));
    // Nothing to run is a success.
    REQUIRE(exec(shippedRooms(), {}).exitCode == 0);
}

TEST_CASE("testExecFile") {
    const TempDirectory testDirectory("pointclick-cli-tests");
    const std::filesystem::path script = testDirectory.path() / "chave.txt";
    std::ofstream(script, std::ios::binary)
            << "# destrava a caixa vermelha\r\n\r\n  give brass_key  \r\nuse brass_key red_box\r\n"
               "# fim\r\nstate";
    const ExecRun run = exec(shippedRooms(),
                             {line("flag visits 1"), file(script), line("take brass_key")});
    REQUIRE(run.exitCode == 1); // the rule took the key: `take` fails
    REQUIRE(run.printed("> give brass_key"));
    REQUIRE(run.printed("flags: red_box_unlocked=1, visits=1"));
    REQUIRE(run.printed("erro: Item fora do inventario: brass_key"));
    REQUIRE(run.lines.back() == "5 comando(s), 1 falha(s)");
    REQUIRE(!run.printed("> # fim"));

    const ExecRun missing = exec(shippedRooms(),
                                 {line("state"), file(testDirectory.path() / "nao.txt")});
    REQUIRE((missing.exitCode == 1 && missing.lines.size() == 1));
    REQUIRE(missing.lines[0] == "erro: Nao foi possivel abrir nao.txt");
}
#else
// A store build has no tools: --exec is not an option there.
TEST_CASE("testParseExecOptions") {
    REQUIRE(parse({"--exec", "state"}).error == "Opcao desconhecida: --exec");
    REQUIRE(commandLineUsage().find("--exec") == std::string_view::npos);
}
#endif
} // namespace
