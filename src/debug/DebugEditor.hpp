#pragma once

#include "content/RoomLibrary.hpp"
#include "debug/CommandConsole.hpp"
#include "input/InputRouter.hpp"
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

class Game;

// Input and viewport hit-testing only: the typed line goes to a CommandConsole (the commands
// are in debug/CommandCatalog) and its answer becomes the status. Drawing lives in
// debug/DebugOverlay.cpp.
// Optional tools layer: built only with POINTCLICK_ENABLE_TOOLS and registered on InputRouter.
class DebugEditor final : public ToolInputConsumer {
public:
    static constexpr SDL_FRect kPanelRect{724.0F, 12.0F, 544.0F, 696.0F};

    // save/load go through the same library the game uses (shared player art, layout).
    // The library must outlive the editor.
    void setLibrary(const RoomLibrary& library) {
        library_ = &library;
        console_.setLibrary(library);
    }
    // Called by `load` and `reload` so PNGs changed on disk are read again. The application
    // wires it to the renderer's texture cache; unset, those commands only skip the reload.
    void setArtReloader(std::function<void()> reloadArt) {
        console_.setArtReloader(std::move(reloadArt));
    }
    // Where `newgame` starts: the run's --room <sala> [spawn], or first.room.
    void setStartRoom(std::string roomId, std::string spawnId = {}) {
        console_.setStartRoom(std::move(roomId), std::move(spawnId));
    }
    // Returns true when the editor consumed the text/key/click.
    bool onTextInput(std::string_view text, Game& game) override;
    bool onToolAction(ToolAction action, Game& game) override;
    [[nodiscard]] bool consumesViewportPoint(SDL_FPoint viewportPoint) const override;

    [[nodiscard]] static constexpr SDL_FRect panelRect() { return kPanelRect; }
    [[nodiscard]] bool active() const { return active_; }
    [[nodiscard]] bool panelVisible() const { return active_ && panelVisible_; }
    [[nodiscard]] const DebugViews& views() const { return console_.views(); }
    [[nodiscard]] std::size_t selected() const { return selected_; }
    [[nodiscard]] const std::string& command() const { return command_; }
    [[nodiscard]] const std::string& status() const { return status_; }
    [[nodiscard]] std::filesystem::path roomDirectory() const {
        return library_ ? library_->roomsDirectory() : std::filesystem::path{};
    }

private:
    void activate(Game& game);
    void submit(Game& game);

    bool active_ = false;
    bool panelVisible_ = true;
    std::size_t selected_ = 0;
    std::string activation_;
    std::string command_;
    std::string status_;
    const RoomLibrary* library_ = nullptr;
    CommandConsole console_;
};
