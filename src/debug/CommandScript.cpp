#include "debug/CommandScript.hpp"
#include "content/RoomLibrary.hpp"
#include "content/TextFormat.hpp"
#include "debug/CommandConsole.hpp"
#include "game/Game.hpp"
#include "game/Identifier.hpp"

#include <exception>
#include <memory>
#include <optional>
#include <utility>
#include <variant>

namespace {
// The main loop's fixed tick (core/Application.cpp), so a settled run is the same simulation.
constexpr float kTickSeconds = 1.0F / 60.0F;
// A walk across a wide room plus a few lines of speech fit well inside this; a game still busy
// after it is reported instead of looping forever.
constexpr int kMaxSettleTicks = 60 * 300;
} // namespace

CommandScript::CommandScript(Game& game, CommandConsole& console, const RoomLibrary& library,
                             CommandPrinter print)
        : game_(game), console_(console), library_(library), print_(std::move(print)) {}

bool CommandScript::run(const std::string_view line) {
    print_("> " + std::string(line));
    const CommandResult result = console_.execute(line, game_);
    if (!result.output.empty()) printOutput(result.output, !result.ok);
    const bool settled = settle();
    return result.ok && settled;
}

void CommandScript::printOutput(const std::string_view text, const bool failed) const {
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = text.find('\n', start);
        const std::string_view line = text.substr(
                start, end == std::string_view::npos ? std::string_view::npos : end - start);
        print_((failed ? "erro: " : "") + std::string(line));
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
}

bool CommandScript::settle() {
    if (!processEvents()) return false;
    for (int tick = 0; tick < kMaxSettleTicks; ++tick) {
        if (!game_.inCutscene() && !game_.scene().player().moving()
            && !game_.scene().pendingInteraction()) {
            return true;
        }
        game_.tick(kTickSeconds);
        if (!processEvents()) return false;
    }
    printOutput("o jogo continuou ocupado depois de " + std::to_string(kMaxSettleTicks / 60)
                        + " s simulados",
                true);
    return false;
}

bool CommandScript::processEvents() {
    std::vector<GameEvent> events;
    game_.drainEvents(events);
    std::optional<RoomChangeRequested> nextRoom;
    for (GameEvent& event : events) {
        if (const auto* line = std::get_if<LineSpoken>(&event)) {
            print_("fala: " + line->text);
        } else if (const auto* entered = std::get_if<RoomEntered>(&event)) {
            print_("sala: " + entered->roomId
                   + (entered->spawnId.empty() ? "" : " em " + entered->spawnId));
        } else if (auto* change = std::get_if<RoomChangeRequested>(&event)) {
            nextRoom = std::move(*change); // several in one drain: the last request wins
        }
    }
    if (!nextRoom) return true;
    // The same checks and the same loading as Application::processGameEvents.
    if (!validIdentifier(nextRoom->roomId)
        || (!nextRoom->spawnId.empty() && !validIdentifier(nextRoom->spawnId))) {
        printOutput("troca de sala invalida: " + nextRoom->roomId + " " + nextRoom->spawnId, true);
        return false;
    }
    const std::string file = nextRoom->roomId + ".room";
    RoomBundleResult loaded = library_.loadBundle(file);
    std::string error = std::move(loaded.error);
    if (!loaded.bundle || !enterRoom(game_, std::move(*loaded.bundle), nextRoom->spawnId, error)) {
        printOutput(file + ": " + error, true);
        return false;
    }
    return processEvents(); // RoomEntered of the room just loaded
}

bool expandCommandSources(const std::span<const CommandSource> sources,
                          std::vector<std::string>& commands, std::string& error) {
    for (const CommandSource& source : sources) {
        if (source.file.empty()) {
            commands.push_back(source.command);
            continue;
        }
        const TextFormat::TextLoad loaded = TextFormat::readFile(source.file);
        if (!loaded.text) {
            error = loaded.error;
            return false;
        }
        const std::string_view text = *loaded.text;
        std::size_t start = 0;
        while (start < text.size()) {
            std::size_t end = text.find('\n', start);
            if (end == std::string_view::npos) end = text.size();
            std::string line = TextFormat::trim(text.substr(start, end - start));
            if (!line.empty() && !line.starts_with('#')) commands.push_back(std::move(line));
            start = end + 1;
        }
    }
    return true;
}

int runCommandScript(const std::filesystem::path& roomsDirectory, const std::string_view startRoom,
                     const std::string_view startSpawn,
                     const std::span<const CommandSource> sources, const CommandPrinter& print) {
    std::vector<std::string> commands;
    std::string error;
    if (!expandCommandSources(sources, commands, error)) {
        print("erro: " + error);
        return 1;
    }
    RoomLibrary library;
    if (!library.open(roomsDirectory, error)) {
        print("erro: " + error);
        return 1;
    }
    const std::string roomFile = startRoom.empty() ? std::string(RoomLibrary::startRoomFile)
                                                   : std::string(startRoom) + ".room";
    RoomBundleResult loaded = library.loadBundle(roomFile);
    if (!loaded.bundle) {
        print("erro: " + roomFile + ": " + loaded.error);
        return 1;
    }
    const std::optional<RoomSpawn> entry = loaded.bundle->room.entryPoint(startSpawn);
    if (!entry) {
        print("erro: Spawn desconhecido na sala " + loaded.bundle->room.id + ": "
              + std::string(startSpawn));
        return 1;
    }
    // A new game, as Application::startGameplay builds one; the save is never read or written.
    std::unique_ptr<Game> game;
    try {
        game = std::make_unique<Game>(std::move(loaded.bundle->room));
    } catch (const std::exception& exception) {
        print("erro: " + std::string(exception.what()));
        return 1;
    }
    if (!game->setInteractions(std::move(loaded.bundle->interactions), error)
        || !game->setDialogues(std::move(loaded.bundle->dialogues), error)) {
        print("erro: " + error);
        return 1;
    }
    if (!startSpawn.empty()) (void)game->placePlayerNear(entry->feet, entry->facing);

    CommandConsole console;
    console.setLibrary(library);
    console.setStartRoom(game->scene().room().id, std::string(startSpawn));
    console.begin(*game);
    CommandScript script(*game, console, library, print);
    std::size_t failures = script.settle() ? 0 : 1;
    for (const std::string& command : commands) {
        if (!script.run(command)) ++failures;
    }
    print(std::to_string(commands.size()) + " comando(s), " + std::to_string(failures)
          + " falha(s)");
    return failures == 0 ? 0 : 1;
}
