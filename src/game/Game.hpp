#pragma once

#include "game/FloatingText.hpp"

#include <string_view>

class Game {
public:
    void onTextInput(std::string_view text);
    void tick(float deltaSeconds);

    [[nodiscard]] const FloatingText& floatingText() const;

private:
    FloatingText floatingText_;
};
