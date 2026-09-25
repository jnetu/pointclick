#include "game/Game.hpp"

#include "game/World.hpp"

#include <utility>

Game::Game(Room room)
        : room_(std::move(room)), player_(room_.playerStart) {
    configureNavigation();
}

void Game::loadRoom(Room room) {
    room_ = std::move(room);
    player_ = Player(room_.playerStart);
    configureNavigation();
}

void Game::configureNavigation() {
    navigation_ = Navigation{};
    navigation_.setWalkableTop(room_.walkableTopY);
    for (const SceneObject& object : room_.scenery) {
        if (object.type == SceneObjectType::solid) {
            navigation_.addObstacle(object.worldCollisionFootprint(),
                                    room_.collisionPaddingX, room_.collisionPaddingY);
        }
    }
}

void Game::onPointerMove(const SDL_FPoint position) {
    pointer_ = position;
    pointerInside_ = position.x >= 0.0F && position.x < World::width
                     && position.y >= 0.0F && position.y < World::height;
}

void Game::onClick(const SDL_FPoint position) {
    onPointerMove(position);
    if (!pointerInside_ || !room_.containsFloor(position)) {
        return;
    }

    const SDL_FPoint destination = room_.clampDestination(position);
    player_.setPath(navigation_.findPathToNearestReachable(player_.feet(), destination));
}

void Game::onTextInput(const std::string_view text) {
    floatingText_.show(text);
}

void Game::tick(const float deltaSeconds) {
    floatingText_.tick(deltaSeconds);
    player_.tick(deltaSeconds, room_.speedAt(player_.feet().y));
}

const FloatingText& Game::floatingText() const {
    return floatingText_;
}

const Player& Game::player() const { return player_; }

SDL_FPoint Game::pointer() const { return pointer_; }

bool Game::pointerInside() const { return pointerInside_; }

const Room& Game::room() const { return room_; }
