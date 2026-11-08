#pragma once

#include "game/SavedGame.hpp"

#include <optional>

class Game;

// Decides when the shell writes the autosave and what goes in it; no I/O here.
// - The snapshot is the last *stable* one: capture() ignores a game held by a cutscene or a
//   dialogue, so a reload never resumes half a puzzle sequence.
// - enterBackground() answers once per trip to the background (SDL sends WILL and DID; both
//   arrive as the same signal), and nothing is rewritten when the file already holds it.
class AutosavePolicy {
public:
    // Stable points (RoomEntered, CutsceneEnded, a lifecycle edge outside a cutscene).
    void capture(const Game& game);

    // Lifecycle edges. Each returns the snapshot to write now, or nullptr: no stable snapshot
    // yet, already written, or a repeated background signal. Call markWritten() after a
    // successful write so the next edge with the same state skips the disk.
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
