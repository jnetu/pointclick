#include "debug/DebugEditor.hpp"
#include "content/InteractionFiles.hpp"
#include "content/RoomFiles.hpp"
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

bool validFilename(const std::string& name) {
    const std::filesystem::path path(name);
    return path.extension() == ".room" && !path.stem().empty()
           && validIdentifier(path.stem().string())
           && path.filename() == path;
}
}

void DebugEditor::setRoomDirectory(std::filesystem::path path) { roomDirectory_ = std::move(path); }

void DebugEditor::activate(Game& game) {
    active_ = true;
    panelVisible_ = true;
    baseline_ = game.room();
    baselineInteractions_ = game.interactions();
    undo_.reset();
    undoInteractions_.reset();
    activation_.clear();
    command_.clear();
    status_ = "Editor ativo. Alteracoes so persistem com save.";
    game.onTextInput("");
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
                         std::optional<RoomInteractions> nextInteractions) {
    if (nextInteractions) {
        status_ = nextInteractions->validationError(&room);
        if (!status_.empty()) return false;
    }
    Room previous = game.room();
    RoomInteractions previousInteractions = game.interactions();
    if (!game.applyRoom(std::move(room), resetPlayer, status_)) return false;
    if (nextInteractions) {
        std::string rulesError;
        (void)game.setInteractions(std::move(*nextInteractions), rulesError);
    }
    undo_ = std::move(previous);
    undoInteractions_ = std::move(previousInteractions);
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
    Room room = game.room();
    parameter->write(room, value);
    commit(game, std::move(room));
}

bool DebugEditor::onKey(const DebugKey key, Game& game) {
    if (key == DebugKey::toggleEditor) {
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
    if (!panelVisible_ && key != DebugKey::togglePanel && key != DebugKey::close) return true;
    const auto parameters = roomParameters();
    switch (key) {
    case DebugKey::toggleEditor: break;
    case DebugKey::togglePanel: panelVisible_ = !panelVisible_; command_.clear(); break;
    case DebugKey::close: active_ = false; command_.clear(); activation_.clear(); break;
    case DebugKey::previous: selected_ = (selected_ + parameters.size() - 1) % parameters.size(); break;
    case DebugKey::next: selected_ = (selected_ + 1) % parameters.size(); break;
    case DebugKey::decrease:
    case DebugKey::increase: {
        const auto& parameter = parameters[selected_];
        setParameter(game, parameter.key, parameter.read(game.room())
                     + (key == DebugKey::increase ? parameter.step : -parameter.step));
        break;
    }
    case DebugKey::backspace: if (!command_.empty()) command_.pop_back(); break;
    case DebugKey::submit: submit(game); command_.clear(); break;
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
        if (!(input >> name) || !finished(input) || !validFilename(name)) {
            status_ = "Use: " + verb + " nome.room (sem pastas)";
            return;
        }
        if (verb == "save") {
            if (RoomFiles::save(roomDirectory_ / name, game.room(), status_)) status_ = "Salvo: " + name;
        } else {
            const auto roomPath = roomDirectory_ / name;
            auto result = RoomFiles::load(roomPath);
            if (!result.room) {
                status_ = result.error;
            } else {
                auto rulesResult = InteractionFiles::loadOptionalForRoom(roomPath, &*result.room);
                if (!rulesResult.interactions) {
                    status_ = rulesResult.error;
                } else if (commit(game, std::move(*result.room), true,
                                  std::move(*rulesResult.interactions))) {
                    status_ = "Sala carregada: " + name;
                }
            }
        }
    } else if (verb == "undo" && finished(input)) {
        if (undo_) commit(game, *undo_, false, undoInteractions_);
        else status_ = "Nenhuma edicao para desfazer";
    } else if (verb == "reset" && finished(input)) {
        if (baseline_) commit(game, *baseline_, true, baselineInteractions_);
    } else if (verb == "move" || verb == "size" || verb == "base" || verb == "solid") {
        std::string id;
        input >> id;
        Room room = game.room();
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
