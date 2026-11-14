#include "debug/CommandConsole.hpp"
#include "content/RoomLibrary.hpp"
#include "debug/CommandCatalog.hpp"
#include "game/Game.hpp"
#include "game/RoomParameters.hpp"

#include <cmath>
#include <locale>
#include <sstream>

CommandConsole::CommandConsole() : startRoom_(RoomLibrary::roomIdFor(RoomLibrary::startRoomFile)) {}

void CommandConsole::setStartRoom(std::string roomId, std::string spawnId) {
    startRoom_ = std::move(roomId);
    startSpawn_ = std::move(spawnId);
}

void CommandConsole::begin(const Game& game) {
    baseline_ = game.scene().room();
    baselineInteractions_ = game.scene().interactions();
    baselineDialogues_ = game.scene().dialogues();
    undo_.reset();
    undoInteractions_.reset();
    undoDialogues_.reset();
    knownRevision_ = game.scene().roomRevision();
}

void CommandConsole::followGame(const Game& game) {
    if (baseline_ && game.scene().roomRevision() != knownRevision_) begin(game);
}

CommandResult CommandConsole::execute(const std::string_view line, Game& game) {
    std::istringstream input{std::string(line)};
    input.imbue(std::locale::classic());
    std::string verb;
    input >> verb;
    CommandResult result;
    if (verb.empty()) return result;
    followGame(game);
    const CommandDescriptor* command = findCommand(verb);
    if (!command) {
        result.ok = false;
        result.output = "Comando desconhecido. Digite help.";
        return result;
    }
    CommandContext context{game, *this, input, result.output, *command};
    result.ok = command->run(context);
    return result;
}

bool CommandConsole::reloadArt() const {
    if (!reloadArt_) return false;
    reloadArt_();
    return true;
}

bool CommandConsole::commit(Game& game, Room room, std::string& output, const bool resetPlayer,
                            std::optional<RoomInteractions> nextInteractions,
                            std::optional<RoomDialogues> nextDialogues,
                            const std::string_view spawnId) {
    if (nextInteractions) {
        output = nextInteractions->validationError(&room);
        if (!output.empty()) return false;
    }
    if (nextDialogues) {
        output = nextDialogues->validationError(&room);
        if (!output.empty()) return false;
    }
    Room previous = game.scene().room();
    RoomInteractions previousInteractions = game.scene().interactions();
    RoomDialogues previousDialogues = game.scene().dialogues();
    if (!game.applyRoom(std::move(room), resetPlayer, output, spawnId)) return false;
    if (nextInteractions) {
        std::string rulesError;
        (void)game.setInteractions(std::move(*nextInteractions), rulesError);
    }
    if (nextDialogues) {
        std::string dialoguesError;
        (void)game.setDialogues(std::move(*nextDialogues), dialoguesError);
    }
    knownRevision_ = game.scene().roomRevision();
    undo_ = std::move(previous);
    undoInteractions_ = std::move(previousInteractions);
    undoDialogues_ = std::move(previousDialogues);
    output = "Aplicado. Rota reiniciada; save para gravar.";
    return true;
}

bool CommandConsole::setParameter(Game& game, const std::string_view key, const float value,
                                  std::string& output) {
    const auto* parameter = findRoomParameter(key);
    if (!parameter) {
        output = "Parametro desconhecido: " + std::string(key);
        return false;
    }
    if (!std::isfinite(value) || value < parameter->minimum || value > parameter->maximum) {
        output = "Valor fora do intervalo permitido";
        return false;
    }
    Room room = game.scene().room();
    parameter->write(room, value);
    return commit(game, std::move(room), output);
}

bool CommandConsole::undo(Game& game, std::string& output) {
    followGame(game);
    if (!undo_) {
        output = "Nenhuma edicao para desfazer";
        return false;
    }
    return commit(game, *undo_, output, false, undoInteractions_, undoDialogues_);
}

bool CommandConsole::reset(Game& game, std::string& output) {
    followGame(game);
    // begin() always runs first (editor activation, headless start); without it there is no
    // reference room and nothing changes.
    if (!baseline_) return true;
    return commit(game, *baseline_, output, true, baselineInteractions_, baselineDialogues_);
}
