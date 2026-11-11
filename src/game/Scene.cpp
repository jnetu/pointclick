#include "game/Scene.hpp"

#include "game/SessionState.hpp"

#include <cmath>
#include <limits>
#include <utility>

namespace {
constexpr float kNoWalkDistance = 0.01F;
// Already within reach of the target, a walk shorter than one navigation cell would only shuffle
// the feet to the centre of their cell before interacting.
constexpr float kSettleDistance = static_cast<float>(Navigation::cellSize);

float distance(const SDL_FPoint a, const SDL_FPoint b) {
    return std::hypot(a.x - b.x, a.y - b.y);
}
} // namespace

Scene::Scene(const SessionState& session)
        : session_(session), player_(room_.playerStart) {}

void Scene::buildNavigation(const Room& room, Navigation& target) const {
    target.reset(room.width, room.height, room.walkableTopY);
    for (const SceneObject& object : room.scenery) {
        if (session_.isObjectSolid(room.id, object)) {
            target.addObstacle(effectiveCollisionFootprint(object),
                               room.collisionPaddingX, room.collisionPaddingY);
        }
    }
}

std::optional<SDL_FPoint> Scene::findNearestWalkable(const Room& room,
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
            const float squared = dx * dx + dy * dy;
            if (navigation.canStandAt(point) && squared < bestDistance) {
                bestDistance = squared;
                nearest = point;
            }
        }
    }
    return nearest;
}

bool Scene::apply(Room room, const bool resetPlayer, std::string& error,
                  const std::string_view spawnId) {
    error = room.validationError();
    if (!error.empty()) return false;
    SDL_FPoint start = player_.feet();
    Facing facing = player_.facing();
    if (resetPlayer) {
        const std::optional<RoomSpawn> entry = room.entryPoint(spawnId);
        if (!entry) {
            error = "Spawn desconhecido na sala " + room.id + ": " + std::string(spawnId);
            return false;
        }
        start = entry->feet;
        facing = entry->facing;
    }
    buildNavigation(room, scratchNavigation_);
    SDL_FPoint feet = room.clampDestination(start);
    if (!scratchNavigation_.canStandAt(feet)) {
        const auto nearest = findNearestWalkable(room, scratchNavigation_, feet);
        if (!nearest) { error = "A sala precisa de uma area caminhavel"; return false; }
        feet = *nearest;
    }
    ObjectAnimations animations;
    animations.rebuild(room, session_, resetPlayer ? nullptr : &objectAnimations_);
    room_ = std::move(room);
    std::swap(navigation_, scratchNavigation_);
    player_ = Player(feet, facing);
    camera_.configure(room_.width, room_.height);
    if (resetPlayer) {
        camera_.snapTo(player_.feet());
        speech_.hide(); // a line still on screen belongs to where the player came from
    }
    objectAnimations_ = std::move(animations);
    pendingInteraction_.reset();
    ++roomRevision_;
    return true;
}

bool Scene::setInteractions(RoomInteractions interactions, std::string& error) {
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

bool Scene::setDialogues(RoomDialogues dialogues, std::string& error) {
    error = dialogues.validationError(&room_);
    if (!error.empty()) {
        return false;
    }
    dialogues_ = std::move(dialogues);
    return true;
}

void Scene::clearContent() {
    interactions_.rules.clear();
    dialogues_.nodes.clear();
}

void Scene::refreshNavigation() {
    buildNavigation(room_, navigation_);
    if (!navigation_.canStandAt(player_.feet())) {
        if (const auto nearest = findNearestWalkable(room_, navigation_, player_.feet())) {
            player_ = Player(*nearest, player_.facing());
        }
    }
}

bool Scene::teleportPlayer(const SDL_FPoint position, std::string& error) {
    const auto feet = room_.clampDestination(position);
    if (!navigation_.canStandAt(feet)) { error = "Posicao bloqueada"; return false; }
    player_ = Player(feet, player_.facing());
    pendingInteraction_.reset();
    camera_.snapTo(player_.feet());
    error.clear();
    return true;
}

SDL_FPoint Scene::placePlayerNear(const SDL_FPoint position,
                                  const std::optional<Facing> facing) {
    SDL_FPoint feet = room_.clampDestination(position);
    if (!navigation_.canStandAt(feet)) {
        const auto nearest = findNearestWalkable(room_, navigation_, feet);
        if (!nearest) return player_.feet();
        feet = *nearest;
    }
    player_ = Player(feet, facing.value_or(player_.facing()));
    pendingInteraction_.reset();
    camera_.snapTo(player_.feet());
    return feet;
}

bool Scene::playObjectAnimation(const std::string_view objectId,
                                const std::string_view animation, const bool restart,
                                std::string& error) {
    return objectAnimations_.play(room_, objectId, animation, restart, error);
}

bool Scene::canResolve(const InteractionTrigger& trigger) const {
    const SceneObject* found = room_.findObject(trigger.objectId);
    if (!found || !isObjectVisible(found->id)) {
        return false;
    }
    return trigger.kind != InteractionTriggerKind::useItem || session_.hasItem(trigger.itemId);
}

Scene::Approach Scene::approach(const InteractionTrigger& trigger) {
    if (!canResolve(trigger)) {
        return Approach::failed;
    }
    const SceneObject& target = *room_.findObject(trigger.objectId);
    const SDL_FPoint desired = interactionApproachPoint(room_, target, session_);
    auto route = navigation_.findPathToNearestReachable(player_.feet(), desired);
    if (route.empty()) {
        pendingInteraction_.reset();
        player_.setPath({});
        return isWithinInteractionReach(room_, target, session_, player_.feet())
               ? Approach::arrived : Approach::failed;
    }

    const float walkDistance = distance(route.back(), player_.feet());
    if (!isWithinInteractionReach(room_, target, session_, route.back())) {
        // Out of reach even at the end of the route: walk as close as possible, no interaction.
        pendingInteraction_.reset();
        player_.setPath(walkDistance > kNoWalkDistance ? std::move(route)
                                                       : std::vector<SDL_FPoint>{});
        return Approach::failed;
    }
    if (walkDistance <= kNoWalkDistance
        || (walkDistance < kSettleDistance
            && isWithinInteractionReach(room_, target, session_, player_.feet()))) {
        pendingInteraction_.reset();
        player_.setPath({});
        return Approach::arrived;
    }
    pendingInteraction_ = PendingInteraction{trigger.objectId, trigger.kind, trigger.itemId};
    player_.setPath(std::move(route));
    return Approach::walking;
}

bool Scene::walkToObject(const std::string_view objectId) {
    const SceneObject* object = room_.findObject(objectId);
    if (!object || !isObjectVisible(object->id)) return false;
    const SDL_FPoint desired = interactionApproachPoint(room_, *object, session_);
    auto route = navigation_.findPathToNearestReachable(player_.feet(), desired);
    if (route.empty() || distance(route.back(), player_.feet()) <= kNoWalkDistance) return false;
    pendingInteraction_.reset();
    player_.setPath(std::move(route));
    return true;
}

void Scene::walkToFloor(const SDL_FPoint position) {
    if (!room_.containsFloor(position)) {
        return;
    }
    pendingInteraction_.reset();
    const SDL_FPoint destination = room_.clampDestination(position);
    player_.setPath(navigation_.findPathToNearestReachable(player_.feet(), destination));
}

void Scene::stopPlayer() {
    pendingInteraction_.reset();
    player_.setPath({});
}

const SceneObject* Scene::pick(const SDL_FPoint worldPoint) const {
    return pickInteractiveObject(room_, session_, interactions_, worldPoint);
}

void Scene::onPointerMove(const SDL_FPoint position, const bool insideViewport) {
    pointer_ = position;
    pointerInside_ = insideViewport
                     && position.x >= 0.0F && position.x < room_.width
                     && position.y >= 0.0F && position.y < room_.height;
}

std::optional<InteractionTrigger> Scene::tick(const float deltaSeconds) {
    speech_.tick(deltaSeconds);
    objectAnimations_.tick(deltaSeconds);
    player_.tick(deltaSeconds, room_.speedAt(player_.feet().y));
    if (pendingInteraction_ && !player_.moving()) {
        const InteractionTrigger trigger = pendingInteraction_->toTrigger();
        pendingInteraction_.reset();
        return trigger;
    }
    return std::nullopt;
}

void Scene::followPlayer(const float deltaSeconds) {
    camera_.tick(deltaSeconds, player_.feet());
}

bool Scene::isObjectVisible(const std::string_view objectId) const {
    return session_.isObjectVisible(room_.id, objectId);
}

bool Scene::isObjectSolid(const SceneObject& object) const {
    return session_.isObjectSolid(room_.id, object);
}

const AnimationPlayback* Scene::objectAnimation(const std::string_view objectId) const {
    return objectAnimations_.find(objectId);
}

bool Scene::animationFinished(const std::string_view objectId) const {
    const SceneObject* object = room_.findObject(objectId);
    const AnimationPlayback* playback = objectAnimations_.find(objectId);
    if (!object || !playback) return true;
    const SpriteClip* clip = findAnimation(object->animations, playback->name());
    return !clip || clip->loop || playback->finished(*clip);
}
