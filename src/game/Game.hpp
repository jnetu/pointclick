#pragma once

#include "game/FloatingText.hpp"
#include "game/Navigation.hpp"
#include "game/Player.hpp"
#include "game/Room.hpp"

#include <string_view>

class Game {
public:
    explicit Game(Room room = Room::firstRoom());
    void loadRoom(Room room);

    void onTextInput(std::string_view text);
    void onPointerMove(SDL_FPoint position);
    void onClick(SDL_FPoint position);
    void tick(float deltaSeconds);

    [[nodiscard]] const FloatingText& floatingText() const;
    [[nodiscard]] const Player& player() const;
    [[nodiscard]] SDL_FPoint pointer() const;
    [[nodiscard]] bool pointerInside() const;
    [[nodiscard]] const Room& room() const;

private:
    void configureNavigation();

    FloatingText floatingText_;
    Room room_;
    Navigation navigation_;
    Player player_;
    SDL_FPoint pointer_{};
    bool pointerInside_ = false;
};
