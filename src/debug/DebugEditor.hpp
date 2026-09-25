#pragma once

#include "game/Room.hpp"
#include <filesystem>
#include <optional>
#include <string_view>

class Game;
enum class DebugKey { previous, next, decrease, increase, submit, backspace, close, togglePanel };

// Input and editing only. Drawing lives in graphics/DebugOverlay.cpp.
class DebugEditor {
public:
    void setRoomDirectory(std::filesystem::path path);
    // Returns true when the editor consumed the text/key.
    bool onTextInput(std::string_view text, Game& game);
    bool onKey(DebugKey key, Game& game);

    [[nodiscard]] bool active() const { return active_; }
    [[nodiscard]] bool panelVisible() const { return active_ && panelVisible_; }
    [[nodiscard]] std::size_t selected() const { return selected_; }
    [[nodiscard]] const std::string& command() const { return command_; }
    [[nodiscard]] const std::string& status() const { return status_; }
    [[nodiscard]] const std::filesystem::path& roomDirectory() const { return roomDirectory_; }

private:
    void submit(Game& game);
    bool commit(Game& game, Room room, bool resetPlayer = false);
    void setParameter(Game& game, std::string_view key, float value);

    bool active_ = false;
    bool panelVisible_ = true;
    std::size_t selected_ = 0;
    std::string activation_;
    std::string command_;
    std::string status_;
    std::filesystem::path roomDirectory_;
    std::optional<Room> baseline_;
    std::optional<Room> undo_;
};
