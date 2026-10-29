#include "game/Game.hpp"

#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>

Game::Game(Room room)
        : player_(room.playerStart) {
    loadRoom(std::move(room));
}

void Game::loadRoom(Room room) {
    std::string error;
    if (!applyRoom(std::move(room), true, error)) throw std::invalid_argument(error);
}

bool Game::applyRoom(Room room, const bool resetPlayer, std::string& error) {
    error = room.validationError();
    if (!error.empty()) return false;
    scratchNavigation_.reset(room.width, room.height, room.walkableTopY);
    for (const SceneObject& object : room.scenery) {
        if (object.type == SceneObjectType::solid) {
            scratchNavigation_.addObstacle(object.worldCollisionFootprint(),
                                           room.collisionPaddingX, room.collisionPaddingY);
        }
    }
    SDL_FPoint feet = room.clampDestination(resetPlayer ? room.playerStart : player_.feet());
    if (!scratchNavigation_.canStandAt(feet)) {
        std::optional<SDL_FPoint> nearest;
        float bestDistance = std::numeric_limits<float>::infinity();
        for (float y = 0.0F; y < room.height; y += Navigation::cellSize) {
            for (float x = 0.0F; x < room.width; x += Navigation::cellSize) {
                const auto point = room.clampDestination({x + Navigation::cellSize * 0.5F,
                                                          y + Navigation::cellSize * 0.5F});
                const float dx = point.x - feet.x;
                const float dy = point.y - feet.y;
                const float distance = dx * dx + dy * dy;
                if (scratchNavigation_.canStandAt(point) && distance < bestDistance) {
                    bestDistance = distance;
                    nearest = point;
                }
            }
        }
        if (!nearest) { error = "A sala precisa de uma area caminhavel"; return false; }
        feet = *nearest;
    }
    ObjectAnimations animations;
    animations.rebuild(room, resetPlayer ? nullptr : &objectAnimations_);
    room_ = std::move(room);
    std::swap(navigation_, scratchNavigation_);
    player_ = Player(feet);
    camera_.configure(room_.width, room_.height);
    if (resetPlayer) {
        camera_.snapTo(player_.feet());
    }
    objectAnimations_ = std::move(animations);
    ++roomRevision_;
    return true;
}

bool Game::teleportPlayer(const SDL_FPoint position, std::string& error) {
    const auto feet = room_.clampDestination(position);
    if (!navigation_.canStandAt(feet)) { error = "Posicao bloqueada"; return false; }
    player_ = Player(feet);
    camera_.snapTo(player_.feet());
    error.clear();
    return true;
}

bool Game::playObjectAnimation(const std::string_view objectId,
                                const std::string_view animation, const bool restart,
                                std::string& error) {
    return objectAnimations_.play(room_, objectId, animation, restart, error);
}

void Game::onPointerMove(const SDL_FPoint position, const bool insideViewport) {
    pointer_ = position;
    pointerInside_ = insideViewport
                     && position.x >= 0.0F && position.x < room_.width
                     && position.y >= 0.0F && position.y < room_.height;
}

void Game::onClick(const SDL_FPoint position) {
    onPointerMove(position, true);
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
    camera_.tick(deltaSeconds, player_.feet());
    objectAnimations_.tick(deltaSeconds);
}

const FloatingText& Game::floatingText() const {
    return floatingText_;
}

const Player& Game::player() const { return player_; }

const Camera& Game::camera() const { return camera_; }

const Navigation& Game::navigation() const { return navigation_; }

SDL_FPoint Game::pointer() const { return pointer_; }

bool Game::pointerInside() const { return pointerInside_; }

const Room& Game::room() const { return room_; }

const AnimationPlayback* Game::objectAnimation(const std::string_view objectId) const {
    return objectAnimations_.find(objectId);
}

unsigned long long Game::roomRevision() const { return roomRevision_; }
