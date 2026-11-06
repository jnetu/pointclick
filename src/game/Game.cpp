#include "game/Game.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>

class Game::Host final : public ScriptHost {
public:
    explicit Host(Game& game) : game_(game) {}

    SessionState& session() override { return game_.session_; }
    std::string_view roomId() const override { return game_.room_.id; }
    void invalidateNavigation() override { game_.navigationDirty_ = true; }

    void say(const std::string_view text) override {
        game_.floatingText_.show(text, FloatingText::readingSeconds(text));
    }
    bool speaking() const override { return game_.floatingText_.isVisible(); }

    void playAnimation(const std::string_view objectId, const std::string_view animation) override {
        std::string ignoredError;
        (void)game_.playObjectAnimation(objectId, animation, true, ignoredError);
    }

    bool animationFinished(const std::string_view objectId) const override {
        const SceneObject* object = game_.room_.findObject(objectId);
        const AnimationPlayback* playback = game_.objectAnimations_.find(objectId);
        if (!object || !playback) return true;
        const SpriteClip* clip = findAnimation(object->animations, playback->name());
        return !clip || clip->loop || playback->finished(*clip);
    }

    bool walkToObject(const std::string_view objectId) override {
        const SceneObject* object = game_.room_.findObject(objectId);
        if (!object || !game_.session_.isObjectVisible(game_.room_.id, object->id)) return false;
        if (game_.navigationDirty_) {
            game_.navigationDirty_ = false;
            game_.refreshNavigation();
        }
        const SDL_FPoint desired = interactionApproachPoint(game_.room_, *object, game_.session_);
        auto route = game_.navigation_.findPathToNearestReachable(game_.player_.feet(), desired);
        if (route.empty()) return false;
        const SDL_FPoint arrival = route.back();
        const SDL_FPoint feet = game_.player_.feet();
        if (std::hypot(arrival.x - feet.x, arrival.y - feet.y) <= 0.01F) return false;
        game_.pendingInteraction_.reset();
        game_.player_.setPath(std::move(route));
        return true;
    }
    bool walking() const override { return game_.player_.moving(); }

    bool startDialogue(const std::string_view nodeId) override {
        return game_.dialogue_.request(game_.dialogues_, nodeId);
    }
    bool dialogueActive() const override { return game_.dialogue_.active(); }

    void changeRoom(const std::string_view roomId) override {
        game_.scriptRoomTarget_ = std::string(roomId);
    }

private:
    Game& game_;
};

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
        // A sequence never crosses rooms: its object references belong to the old one.
        script_.cancel();
        dialogue_.cancel();
        dialogues_.nodes.clear();
        scriptRoomTarget_.reset();
        navigationDirty_ = false;
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

void Game::setDialogues(RoomDialogues dialogues) {
    std::string error;
    if (!setDialogues(std::move(dialogues), error)) {
        throw std::invalid_argument(error);
    }
}

bool Game::setDialogues(RoomDialogues dialogues, std::string& error) {
    error = dialogues.validationError(&room_);
    if (!error.empty()) {
        return false;
    }
    // The player points into dialogues_; steps already pushed to the runner are copies.
    dialogue_.cancel();
    dialogues_ = std::move(dialogues);
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
    const SceneObject* found = room_.findObject(trigger.objectId);
    if (!found || !session_.isObjectVisible(room_.id, found->id)) {
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
    const SceneObject* found = room_.findObject(trigger.objectId);
    if (!found || !session_.isObjectVisible(room_.id, found->id)) {
        return false;
    }
    if (trigger.kind == InteractionTriggerKind::useItem && !session_.hasItem(trigger.itemId)) {
        return false;
    }

    const InteractionRule* rule = interactions_.findMatchingRule(trigger, session_, room_.id);
    if (!rule) {
        return false;
    }
    Host host(*this);
    dialogue_.cancel(); // start() drops every stacked sequence, a dialogue's included
    script_.start(rule->effects, host);
    settleScript();
    return true;
}

void Game::settleScript() {
    Host host(*this);
    dialogue_.settle(script_, host);
    if (navigationDirty_) {
        navigationDirty_ = false;
        refreshNavigation();
    }
    if (!script_.busy() && !dialogue_.active() && scriptRoomTarget_) {
        pendingRoomTransition_ = std::move(*scriptRoomTarget_);
        scriptRoomTarget_.reset();
    }
}

bool Game::startDialogue(const std::string_view nodeId) {
    if (inCutscene() || !dialogue_.request(dialogues_, nodeId)) {
        return false;
    }
    pendingInteraction_.reset();
    player_.setPath({});
    settleScript();
    return true;
}

bool Game::chooseDialogueOption(const std::size_t index) {
    Host host(*this);
    if (!dialogue_.choose(index, script_, host)) {
        return false;
    }
    settleScript();
    return true;
}

bool Game::useItemOnObject(const std::string_view itemId, const std::string_view objectId) {
    if (inCutscene() || !session_.hasItem(itemId)) {
        return false;
    }
    return startObjectInteraction(
            InteractionTrigger::useItem(std::string(itemId), std::string(objectId)));
}

bool Game::useItemAt(const std::string_view itemId, const SDL_FPoint worldPoint) {
    if (inCutscene() || !session_.hasItem(itemId)) {
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
    if (inCutscene()) {
        // Cutscene: a click only skips the line being spoken.
        floatingText_.hide();
        return;
    }
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
    if (script_.busy() || dialogue_.active()) {
        Host host(*this);
        script_.tick(deltaSeconds, host);
        settleScript();
    }
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

bool Game::inCutscene() const { return script_.busy() || dialogue_.active(); }

const RoomDialogues& Game::dialogues() const { return dialogues_; }

bool Game::dialogueActive() const { return dialogue_.active(); }

bool Game::awaitingDialogueChoice() const {
    return dialogue_.phase() == DialoguePhase::choosing;
}

std::span<const DialogueChoice* const> Game::dialogueOptions() const {
    return dialogue_.options();
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
