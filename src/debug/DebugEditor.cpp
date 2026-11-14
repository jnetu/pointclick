#include "debug/DebugEditor.hpp"
#include "game/RoomParameters.hpp"
#include "game/Game.hpp"

#include <cctype>

void DebugEditor::activate(Game& game) {
    active_ = true;
    panelVisible_ = true;
    console_.begin(game);
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
    for (const char raw : text) {
        const auto character = static_cast<unsigned char>(raw);
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
        (void)console_.setParameter(
                game, parameter.key,
                parameter.read(game.scene().room())
                        + (key == ToolAction::increase ? parameter.step : -parameter.step),
                status_);
        break;
    }
    case ToolAction::backspace: if (!command_.empty()) command_.pop_back(); break;
    case ToolAction::submit: submit(game); command_.clear(); break;
    }
    return true;
}

void DebugEditor::submit(Game& game) {
    // A blank line keeps the last status; anything else is answered by the console.
    if (command_.find_first_not_of(' ') == std::string::npos) return;
    status_ = console_.execute(command_, game).output;
}
