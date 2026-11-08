#include "game/Autosave.hpp"

#include "game/Game.hpp"

void AutosavePolicy::capture(const Game& game) {
    if (!game.inCutscene()) stable_ = game.savedGame();
}

const SavedGame* AutosavePolicy::enterBackground(const Game& game) {
    if (background_) return nullptr;
    background_ = true;
    return pending(game);
}

void AutosavePolicy::enterForeground() {
    background_ = false;
}

const SavedGame* AutosavePolicy::quit(const Game& game) {
    return pending(game);
}

void AutosavePolicy::markWritten() {
    written_ = stable_;
}

const SavedGame* AutosavePolicy::pending(const Game& game) {
    capture(game);
    if (!stable_ || stable_ == written_) return nullptr;
    return &*stable_;
}
