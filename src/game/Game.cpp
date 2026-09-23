#include "game/Game.hpp"

void Game::onTextInput(const std::string_view text) {
    floatingText_.show(text);
}

void Game::tick(const float deltaSeconds) {
    floatingText_.tick(deltaSeconds);
}

const FloatingText& Game::floatingText() const {
    return floatingText_;
}
