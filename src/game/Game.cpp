#include "game/Game.hpp"

#include <algorithm>
#include <cmath>
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

void Game::buildNavigation(const Room& room, Navigation& target) const {
    target.reset(room.width, room.height, room.walkableTopY);
    for (const SceneObject& object : room.scenery) {
        if (session_.isObjectSolid(room.id, object)) {
            target.addObstacle(effectiveCollisionFootprint(object),
                               room.collisionPaddingX, room.collisionPaddingY);
        }
    }
}

std::optional<SDL_FPoint> Game::findNearestWalkable(const Room& room,
                                                    const Navigation& navigation,
                                                    const SDL_FPoint desiredFeet) {
    std::optional<SDL_FPoint> nearest;
    float bestDistance = std::numeric_limits<float>::infinity();
    for (float y = 0.0F; y < room.height; y += Navigation::cellSize) {
        for (float x = 0.0F; x < room.width; x += Navigation::cellSize) {
            const auto point = room.clampDestination({x + Navigation::cellSize * 0.5F,
                                                      y + Navigation::cellSize * 0.5F});
            const float dx = point.x - desiredFeet.x;
            const float dy = point.y - desiredFeet.y;
            const float distance = dx * dx + dy * dy;
            if (navigation.canStandAt(point) && distance < bestDistance) {
                bestDistance = distance;
                nearest = point;
            }
        }
    }
    return nearest;
}

bool Game::applyRoom(Room room, const bool resetPlayer, std::string& error) {
    error = room.validationError();
    if (!error.empty()) return false;
    buildNavigation(room, scratchNavigation_);
    SDL_FPoint feet = room.clampDestination(resetPlayer ? room.playerStart : player_.feet());
    if (!scratchNavigation_.canStandAt(feet)) {
        const auto nearest = findNearestWalkable(room, scratchNavigation_, feet);
        if (!nearest) { error = "A sala precisa de uma area caminhavel"; return false; }
        feet = *nearest;
    }
    ObjectAnimations animations;
    animations.rebuild(room, resetPlayer ? nullptr : &objectAnimations_);
    const bool changingRoom = room.id != room_.id;
    room_ = std::move(room);
    std::swap(navigation_, scratchNavigation_);
    player_ = Player(feet);
    camera_.configure(room_.width, room_.height);
    if (resetPlayer) {
        camera_.snapTo(player_.feet());
    }
    objectAnimations_ = std::move(animations);
    pendingInteraction_.reset();
    if (changingRoom) {
        interactions_.rules.clear();
        pendingRoomTransition_.reset();
    }
    ++roomRevision_;
    return true;
}

void Game::setInteractions(RoomInteractions interactions) {
    std::string error;
    if (!setInteractions(std::move(interactions), error)) {
        throw std::invalid_argument(error);
    }
}

bool Game::setInteractions(RoomInteractions interactions, std::string& error) {
    error = interactions.validationError(&room_);
    if (!error.empty()) {
        return false;
    }
    interactions_ = std::move(interactions);
    if (pendingInteraction_ && !interactions_.hasRulesForObject(pendingInteraction_->objectId)) {
        pendingInteraction_.reset();
    }
    return true;
}

void Game::refreshNavigation() {
    buildNavigation(room_, navigation_);
    if (!navigation_.canStandAt(player_.feet())) {
        if (const auto nearest = findNearestWalkable(room_, navigation_, player_.feet())) {
            player_ = Player(*nearest);
        }
    }
}

bool Game::teleportPlayer(const SDL_FPoint position, std::string& error) {
    const auto feet = room_.clampDestination(position);
    if (!navigation_.canStandAt(feet)) { error = "Posicao bloqueada"; return false; }
    player_ = Player(feet);
    pendingInteraction_.reset();
    camera_.snapTo(player_.feet());
    error.clear();
    return true;
}

bool Game::playObjectAnimation(const std::string_view objectId,
                                const std::string_view animation, const bool restart,
                                std::string& error) {
    return objectAnimations_.play(room_, objectId, animation, restart, error);
}

bool Game::startObjectInteraction(const InteractionTrigger& trigger) {
    const auto found = std::find_if(room_.scenery.begin(), room_.scenery.end(),
                                    [&](const SceneObject& obj) {
                                        return obj.id == trigger.objectId;
                                    });
    if (found == room_.scenery.end() || !session_.isObjectVisible(room_.id, found->id)) {
        return false;
    }
    if (trigger.kind == InteractionTriggerKind::useItem && !session_.hasItem(trigger.itemId)) {
        return false;
    }

    const SceneObject& target = *found;
    const SDL_FPoint desired = interactionApproachPoint(room_, target, session_);
    auto route = navigation_.findPathToNearestReachable(player_.feet(), desired);
    if (route.empty()) {
        pendingInteraction_.reset();
        player_.setPath({});
        if (isWithinInteractionReach(room_, target, session_, player_.feet())) {
            return resolveInteraction(trigger);
        }
        return false;
    }

    const SDL_FPoint arrivalFeet = route.back();
    const float walkDistance = std::hypot(arrivalFeet.x - player_.feet().x,
                                          arrivalFeet.y - player_.feet().y);
    if (!isWithinInteractionReach(room_, target, session_, arrivalFeet)) {
        pendingInteraction_.reset();
        if (walkDistance > 0.01F) {
            player_.setPath(std::move(route));
        } else {
            player_.setPath({});
        }
        return false;
    }

    if (walkDistance <= 0.01F) {
        pendingInteraction_.reset();
        player_.setPath({});
        return resolveInteraction(trigger);
    }

    pendingInteraction_ = PendingInteraction{trigger.objectId, trigger.kind, trigger.itemId};
    player_.setPath(std::move(route));
    return true;
}

bool Game::resolveInteraction(const InteractionTrigger& trigger) {
    const auto found = std::find_if(room_.scenery.begin(), room_.scenery.end(),
                                    [&](const SceneObject& obj) {
                                        return obj.id == trigger.objectId;
                                    });
    if (found == room_.scenery.end() || !session_.isObjectVisible(room_.id, found->id)) {
        return false;
    }
    if (trigger.kind == InteractionTriggerKind::useItem && !session_.hasItem(trigger.itemId)) {
        return false;
    }

    const InteractionOutcome outcome = applyInteraction(interactions_, room_.id, trigger, session_);
    if (!outcome.matched) {
        return false;
    }

    if (outcome.spokenText) {
        floatingText_.show(*outcome.spokenText);
    }
    for (const ObjectAnimationRequest& anim : outcome.animations) {
        std::string ignoredError;
        (void)playObjectAnimation(anim.objectId, anim.animation, true, ignoredError);
    }
    if (outcome.navigationChanged) {
        refreshNavigation();
    }
    if (outcome.targetRoomId) {
        pendingRoomTransition_ = *outcome.targetRoomId;
    }
    return true;
}

bool Game::useItemOnObject(const std::string_view itemId, const std::string_view objectId) {
    if (!session_.hasItem(itemId)) {
        return false;
    }
    return startObjectInteraction(
            InteractionTrigger::useItem(std::string(itemId), std::string(objectId)));
}

bool Game::useItemAt(const std::string_view itemId, const SDL_FPoint worldPoint) {
    if (!session_.hasItem(itemId)) {
        return false;
    }
    const SceneObject* target = pickInteractiveObject(room_, session_, interactions_, worldPoint);
    if (!target) {
        return false;
    }
    return startObjectInteraction(
            InteractionTrigger::useItem(std::string(itemId), target->id));
}

void Game::onPointerMove(const SDL_FPoint position, const bool insideViewport) {
    pointer_ = position;
    pointerInside_ = insideViewport
                     && position.x >= 0.0F && position.x < room_.width
                     && position.y >= 0.0F && position.y < room_.height;
}

void Game::onClick(const SDL_FPoint position) {
    onPointerMove(position, true);
    if (!pointerInside_) {
        return;
    }

    if (const SceneObject* object = pickInteractiveObject(room_, session_, interactions_, position)) {
        (void)startObjectInteraction(InteractionTrigger::interact(object->id));
        return;
    }

    if (!room_.containsFloor(position)) {
        return;
    }

    pendingInteraction_.reset();
    const SDL_FPoint destination = room_.clampDestination(position);
    player_.setPath(navigation_.findPathToNearestReachable(player_.feet(), destination));
}

void Game::onTextInput(const std::string_view text) {
    floatingText_.show(text);
}

void Game::tick(const float deltaSeconds) {
    floatingText_.tick(deltaSeconds);
    objectAnimations_.tick(deltaSeconds);
    player_.tick(deltaSeconds, room_.speedAt(player_.feet().y));
    if (pendingInteraction_ && !player_.moving()) {
        const InteractionTrigger trigger = pendingInteraction_->toTrigger();
        pendingInteraction_.reset();
        (void)resolveInteraction(trigger);
    }
    camera_.tick(deltaSeconds, player_.feet());
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

const RoomInteractions& Game::interactions() const { return interactions_; }

SessionState& Game::session() { return session_; }

const SessionState& Game::session() const { return session_; }

bool Game::isObjectVisible(const std::string_view objectId) const {
    return session_.isObjectVisible(room_.id, objectId);
}

bool Game::isObjectSolid(const SceneObject& object) const {
    return session_.isObjectSolid(room_.id, object);
}

const std::optional<PendingInteraction>& Game::pendingInteraction() const {
    return pendingInteraction_;
}

const std::optional<std::string>& Game::pendingRoomTransition() const {
    return pendingRoomTransition_;
}

std::optional<std::string> Game::consumeRoomTransition() {
    auto next = std::move(pendingRoomTransition_);
    pendingRoomTransition_.reset();
    return next;
}

const AnimationPlayback* Game::objectAnimation(const std::string_view objectId) const {
    return objectAnimations_.find(objectId);
}

unsigned long long Game::roomRevision() const { return roomRevision_; }
