#pragma once

#include "game/SavedGame.hpp"

#include <optional>

class Game;

// Decides when the shell writes the autosave and what goes in it; no I/O here.
// Keeps only stable snapshots (never inside a cutscene or dialogue) and answers once per trip to
// the background (SDL sends WILL and DID); unchanged state is never rewritten.
class AutosavePolicy {
public:
    // Stable points (RoomEntered, CutsceneEnded, a lifecycle edge outside a cutscene).
    void capture(const Game& game);

    // Lifecycle edges: the snapshot to write now, or nullptr (none yet, already written, repeated
    // signal). Call markWritten() after a successful write.
    [[nodiscard]] const SavedGame* enterBackground(const Game& game);
    void enterForeground();
    [[nodiscard]] const SavedGame* quit(const Game& game);
    void markWritten();

    [[nodiscard]] const std::optional<SavedGame>& stable() const { return stable_; }
    [[nodiscard]] bool inBackground() const { return background_; }

private:
    [[nodiscard]] const SavedGame* pending(const Game& game);

    std::optional<SavedGame> stable_;
    std::optional<SavedGame> written_;
    bool background_ = false;
};
