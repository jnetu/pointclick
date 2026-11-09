#include "debug/DebugEditor.hpp"
#include "game/RoomParameters.hpp"
#include "game/Game.hpp"
#include "game/Identifier.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <locale>
#include <sstream>

namespace {
bool finished(std::istringstream& input) { input >> std::ws; return input.eof(); }

// "<id>.room" with no folders; checked on the typed text, never through a native path string.
bool validFilename(const std::string_view name) {
    constexpr std::string_view extension = ".room";
    return name.size() > extension.size() && name.ends_with(extension)
           && validIdentifier(name.substr(0, name.size() - extension.size()));
}
}


void DebugEditor::activate(Game& game) {
    active_ = true;
    panelVisible_ = true;
    baseline_ = game.scene().room();
    baselineInteractions_ = game.scene().interactions();
    baselineDialogues_ = game.scene().dialogues();
    undo_.reset();
    undoInteractions_.reset();
    undoDialogues_.reset();
    activation_.clear();
    command_.clear();
    status_ = "Editor ativo. Alteracoes so persistem com save.";
}

bool DebugEditor::consumesViewportPoint(const SDL_FPoint viewportPoint) const {
    return panelVisible() && SDL_PointInRectFloat(&viewportPoint, &kPanelRect);
}

bool DebugEditor::onTextInput(const std::string_view text, Game& game) {
    if (active_ && !panelVisible_) return true;
    bool consumed = active_;
    for (const unsigned char character : text) {
        if (active_) {
            if (character >= 32 && character <= 126 && command_.size() < 120) command_ += static_cast<char>(character);
        } else {
            activation_ += static_cast<char>(std::toupper(character));
            if (activation_.size() > 5) activation_.erase(0, activation_.size() - 5);
            if (activation_ == "DEBUG") {
                activate(game);
                consumed = true;
            }
        }
    }
    return consumed;
}

bool DebugEditor::commit(Game& game, Room room, const bool resetPlayer,
                         std::optional<RoomInteractions> nextInteractions,
                         std::optional<RoomDialogues> nextDialogues,
                         const std::string_view spawnId) {
    if (nextInteractions) {
        status_ = nextInteractions->validationError(&room);
        if (!status_.empty()) return false;
    }
    if (nextDialogues) {
        status_ = nextDialogues->validationError(&room);
        if (!status_.empty()) return false;
    }
    Room previous = game.scene().room();
    RoomInteractions previousInteractions = game.scene().interactions();
    RoomDialogues previousDialogues = game.scene().dialogues();
    if (!game.applyRoom(std::move(room), resetPlayer, status_, spawnId)) return false;
    if (nextInteractions) {
        std::string rulesError;
        (void)game.setInteractions(std::move(*nextInteractions), rulesError);
    }
    if (nextDialogues) {
        std::string dialoguesError;
        (void)game.setDialogues(std::move(*nextDialogues), dialoguesError);
    }
    undo_ = std::move(previous);
    undoInteractions_ = std::move(previousInteractions);
    undoDialogues_ = std::move(previousDialogues);
    status_ = "Aplicado. Rota reiniciada; save para gravar.";
    return true;
}

void DebugEditor::setParameter(Game& game, const std::string_view key, const float value) {
    const auto* parameter = findRoomParameter(key);
    if (!parameter) { status_ = "Parametro desconhecido: " + std::string(key); return; }
    if (!std::isfinite(value) || value < parameter->minimum || value > parameter->maximum) {
        status_ = "Valor fora do intervalo permitido";
        return;
    }
    Room room = game.scene().room();
    parameter->write(room, value);
    commit(game, std::move(room));
}

bool DebugEditor::onToolAction(const ToolAction key, Game& game) {
    if (key == ToolAction::toggleEditor) {
        if (active_) {
            active_ = false;
            command_.clear();
            activation_.clear();
        } else {
            activate(game);
        }
        return true;
    }
    if (!active_) return false;
    // Panel hidden to test the room: only TAB and ESC are the editor's. Arrows and Enter go on
    // to the gameplay UI (dialogue menu).
    if (!panelVisible_ && key != ToolAction::togglePanel && key != ToolAction::close) return false;
    const auto parameters = roomParameters();
    switch (key) {
    case ToolAction::toggleEditor: break;
    case ToolAction::togglePanel: panelVisible_ = !panelVisible_; command_.clear(); break;
    case ToolAction::close: active_ = false; command_.clear(); activation_.clear(); break;
    case ToolAction::previous: selected_ = (selected_ + parameters.size() - 1) % parameters.size(); break;
    case ToolAction::next: selected_ = (selected_ + 1) % parameters.size(); break;
    case ToolAction::decrease:
    case ToolAction::increase: {
        const auto& parameter = parameters[selected_];
        setParameter(game, parameter.key, parameter.read(game.scene().room())
                     + (key == ToolAction::increase ? parameter.step : -parameter.step));
        break;
    }
    case ToolAction::backspace: if (!command_.empty()) command_.pop_back(); break;
    case ToolAction::submit: submit(game); command_.clear(); break;
    }
    return true;
}

void DebugEditor::submit(Game& game) {
    std::istringstream input(command_);
    input.imbue(std::locale::classic());
    std::string verb;
    input >> verb;
    if (verb.empty()) return;
    if (verb == "set") {
        std::string key;
        float value;
        if (input >> key >> value && finished(input)) setParameter(game, key, value);
        else status_ = "Use: set player.far_speed 140";
    } else if (verb == "teleport") {
        SDL_FPoint point{};
        if (input >> point.x >> point.y && finished(input)
            && std::isfinite(point.x) && std::isfinite(point.y)) {
            if (game.teleportPlayer(point, status_)) status_ = "Player reposicionado";
        } else status_ = "Use: teleport 640 500";
    } else if (verb == "save" || verb == "load") {
        std::string name;
        std::string spawn; // load only: enter through a [spawn] of the room
        const bool named = static_cast<bool>(input >> name);
        if (verb == "load" && named && !finished(input)) input >> spawn;
        if (!named || !finished(input) || !validFilename(name)
            || (!spawn.empty() && !validIdentifier(spawn))) {
            status_ = verb == "load" ? "Use: load nome.room [spawn] (sem pastas)"
                                     : "Use: save nome.room (sem pastas)";
            return;
        }
        if (!library_) {
            status_ = "Pasta de salas nao configurada";
        } else if (verb == "save") {
            if (library_->saveRoom(name, game.scene().room(), status_)) status_ = "Salvo: " + name;
        } else if (auto loaded = library_->loadBundle(name); !loaded.bundle) {
            status_ = std::move(loaded.error);
        } else if (commit(game, std::move(loaded.bundle->room), true,
                          std::move(loaded.bundle->interactions),
                          std::move(loaded.bundle->dialogues), spawn)) {
            status_ = "Sala carregada: " + name + (spawn.empty() ? "" : " em " + spawn);
        }
    } else if (verb == "undo" && finished(input)) {
        if (undo_) commit(game, *undo_, false, undoInteractions_, undoDialogues_);
        else status_ = "Nenhuma edicao para desfazer";
    } else if (verb == "reset" && finished(input)) {
        if (baseline_) commit(game, *baseline_, true, baselineInteractions_, baselineDialogues_);
    } else if (verb == "move" || verb == "size" || verb == "base" || verb == "solid") {
        std::string id;
        input >> id;
        Room room = game.scene().room();
        const auto object = std::find_if(room.scenery.begin(), room.scenery.end(),
                                         [&](const auto& item) { return item.id == id; });
        if (object == room.scenery.end()) { status_ = "Objeto desconhecido: " + id; return; }
        bool parsed = false;
        if (verb == "move") parsed = static_cast<bool>(input >> object->bounds.x >> object->bounds.y);
        if (verb == "size") parsed = static_cast<bool>(input >> object->bounds.w >> object->bounds.h);
        if (verb == "base") {
            auto& base = object->collisionFootprint;
            parsed = static_cast<bool>(input >> base.x >> base.y >> base.w >> base.h);
        }
        if (verb == "solid") {
            int enabled = -1;
            parsed = static_cast<bool>(input >> enabled) && (enabled == 0 || enabled == 1);
            object->type = enabled == 1 ? SceneObjectType::solid : SceneObjectType::decoration;
        }
        if (parsed && finished(input)) commit(game, std::move(room));
        else status_ = "Use move/size ID X Y, base ID X Y W H, solid ID 0/1";
    } else if (verb == "help") {
        status_ = "set, teleport, move, size, base, solid, save, load, undo, reset";
    } else {
        status_ = "Comando desconhecido. Digite help.";
    }
}
