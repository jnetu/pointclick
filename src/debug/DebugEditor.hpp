#pragma once

#include "content/RoomLibrary.hpp"
#include "input/InputRouter.hpp"
#include "game/Dialogue.hpp"
#include "game/Interaction.hpp"
#include "game/Room.hpp"
#include <filesystem>
#include <optional>
#include <string_view>

class Game;

// What the tools draw over the game, switched with `show <view> <0|1>`. None of it exists in a
// build without tools, and with the editor closed only the hint is drawn.
struct DebugViews {
    bool route = true;       // route and destination cross, while the editor is active
    bool diagnostics = true; // technical readout, while the editor is active
    bool hint = true;        // how to open the editor, while it is closed
};

// Input, viewport hit-testing, and editing only. Drawing lives in debug/DebugOverlay.cpp.
// Optional tools layer: built only with POINTCLICK_ENABLE_TOOLS and registered on InputRouter.
class DebugEditor final : public ToolInputConsumer {
public:
    static constexpr SDL_FRect kPanelRect{724.0F, 12.0F, 544.0F, 696.0F};

    // save/load go through the same library the game uses (shared player art, layout).
    // The library must outlive the editor.
    void setLibrary(const RoomLibrary& library) { library_ = &library; }
    // Returns true when the editor consumed the text/key/click.
    bool onTextInput(std::string_view text, Game& game) override;
    bool onToolAction(ToolAction action, Game& game) override;
    [[nodiscard]] bool consumesViewportPoint(SDL_FPoint viewportPoint) const override;

    [[nodiscard]] static constexpr SDL_FRect panelRect() { return kPanelRect; }
    [[nodiscard]] bool active() const { return active_; }
    [[nodiscard]] bool panelVisible() const { return active_ && panelVisible_; }
    [[nodiscard]] const DebugViews& views() const { return views_; }
    [[nodiscard]] std::size_t selected() const { return selected_; }
    [[nodiscard]] const std::string& command() const { return command_; }
    [[nodiscard]] const std::string& status() const { return status_; }
    [[nodiscard]] std::filesystem::path roomDirectory() const {
        return library_ ? library_->roomsDirectory() : std::filesystem::path{};
    }

private:
    void activate(Game& game);
    void submit(Game& game);
    bool commit(Game& game, Room room, bool resetPlayer = false,
                std::optional<RoomInteractions> nextInteractions = std::nullopt,
                std::optional<RoomDialogues> nextDialogues = std::nullopt,
                std::string_view spawnId = {});
    void setParameter(Game& game, std::string_view key, float value);

    bool active_ = false;
    bool panelVisible_ = true;
    DebugViews views_;
    std::size_t selected_ = 0;
    std::string activation_;
    std::string command_;
    std::string status_;
    const RoomLibrary* library_ = nullptr;
    std::optional<Room> baseline_;
    std::optional<RoomInteractions> baselineInteractions_;
    std::optional<RoomDialogues> baselineDialogues_;
    std::optional<Room> undo_;
    std::optional<RoomInteractions> undoInteractions_;
    std::optional<RoomDialogues> undoDialogues_;
};
