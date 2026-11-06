#include "game/Game.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <variant>

// Routes what scripted actions ask for to the scene, the session or the dialogue player.
class Game::Host final : public ScriptHost {
public:
    explicit Host(Game& game) : game_(game) {}

    SessionState& session() override { return game_.session_; }
    std::string_view roomId() const override { return game_.scene_.room().id; }
    void invalidateNavigation() override { game_.navigationDirty_ = true; }

    void say(const std::string_view text) override {
        game_.scene_.speech().show(text, FloatingText::readingSeconds(text));
        game_.emit(LineSpoken{std::string(text)});
    }
    bool speaking() const override { return game_.scene_.floatingText().isVisible(); }

    void playAnimation(const std::string_view objectId, const std::string_view animation) override {
        std::string ignoredError;
        (void)game_.scene_.playObjectAnimation(objectId, animation, true, ignoredError);
    }
    bool animationFinished(const std::string_view objectId) const override {
        return game_.scene_.animationFinished(objectId);
    }

    bool walkToObject(const std::string_view objectId) override {
        if (game_.navigationDirty_) {
            game_.navigationDirty_ = false;
            game_.scene_.refreshNavigation();
        }
        return game_.scene_.walkToObject(objectId);
    }
    bool walking() const override { return game_.scene_.player().moving(); }

    bool startDialogue(const std::string_view nodeId) override {
        return game_.dialogue_.request(game_.scene_.dialogues(), nodeId);
    }
    bool dialogueActive() const override { return game_.dialogue_.active(); }

    void changeRoom(const std::string_view roomId) override {
        game_.scriptRoomTarget_ = std::string(roomId);
    }

private:
    Game& game_;
};

Game::Game(Room room) {
    loadRoom(std::move(room));
}

void Game::loadRoom(Room room) {
    std::string error;
    if (!applyRoom(std::move(room), true, error)) throw std::invalid_argument(error);
}

bool Game::applyRoom(Room room, const bool resetPlayer, std::string& error) {
    const bool changingRoom = room.id != scene_.room().id;
    std::string roomId = room.id;
    if (!scene_.apply(std::move(room), resetPlayer, error)) return false;
    if (changingRoom) {
        scene_.clearContent();
        // A sequence never crosses rooms: its object references belong to the old one.
        script_.cancel();
        dialogue_.cancel();
        scriptRoomTarget_.reset();
        navigationDirty_ = false;
        // A request queued for the room we just left is stale now.
        std::erase_if(events_, [](const GameEvent& event) {
            return std::holds_alternative<RoomChangeRequested>(event);
        });
    }
    if (resetPlayer) {
        emit(RoomEntered{std::move(roomId)});
    }
    publishCutsceneChange();
    return true;
}

void Game::setInteractions(RoomInteractions interactions) {
    std::string error;
    if (!setInteractions(std::move(interactions), error)) {
        throw std::invalid_argument(error);
    }
}

bool Game::setInteractions(RoomInteractions interactions, std::string& error) {
    return scene_.setInteractions(std::move(interactions), error);
}

void Game::setDialogues(RoomDialogues dialogues) {
    std::string error;
    if (!setDialogues(std::move(dialogues), error)) {
        throw std::invalid_argument(error);
    }
}

bool Game::setDialogues(RoomDialogues dialogues, std::string& error) {
    // Validate first: a rejected file must not end the conversation in progress.
    error = dialogues.validationError(&scene_.room());
    if (!error.empty()) {
        return false;
    }
    // The player points into the scene's nodes; steps already pushed to the runner are copies.
    dialogue_.cancel();
    const bool applied = scene_.setDialogues(std::move(dialogues), error);
    publishCutsceneChange();
    return applied;
}

bool Game::teleportPlayer(const SDL_FPoint position, std::string& error) {
    return scene_.teleportPlayer(position, error);
}

bool Game::playObjectAnimation(const std::string_view objectId,
                               const std::string_view animation, const bool restart,
                               std::string& error) {
    return scene_.playObjectAnimation(objectId, animation, restart, error);
}

bool Game::startObjectInteraction(const InteractionTrigger& trigger) {
    switch (scene_.approach(trigger)) {
    case Scene::Approach::arrived:
        return resolveInteraction(trigger);
    case Scene::Approach::walking:
        return true;
    case Scene::Approach::failed:
        break;
    }
    return false;
}

bool Game::resolveInteraction(const InteractionTrigger& trigger) {
    if (!scene_.canResolve(trigger)) {
        return false;
    }
    const InteractionRule* rule =
            scene_.interactions().findMatchingRule(trigger, session_, scene_.room().id);
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
        scene_.refreshNavigation();
    }
    if (!script_.busy() && !dialogue_.active() && scriptRoomTarget_) {
        emit(RoomChangeRequested{std::move(*scriptRoomTarget_)});
        scriptRoomTarget_.reset();
    }
    publishCutsceneChange();
}

void Game::publishCutsceneChange() {
    const bool now = inCutscene();
    if (now == wasInCutscene_) return;
    wasInCutscene_ = now;
    if (now) emit(CutsceneStarted{});
    else emit(CutsceneEnded{});
}

void Game::emit(GameEvent event) {
    events_.push_back(std::move(event));
}

void Game::drainEvents(std::vector<GameEvent>& out) {
    out.clear();
    std::swap(out, events_);
}

bool Game::startDialogue(const std::string_view nodeId) {
    if (inCutscene() || !dialogue_.request(scene_.dialogues(), nodeId)) {
        return false;
    }
    scene_.stopPlayer();
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
    const SceneObject* target = scene_.pick(worldPoint);
    if (!target) {
        return false;
    }
    return startObjectInteraction(
            InteractionTrigger::useItem(std::string(itemId), target->id));
}

void Game::onPointerMove(const SDL_FPoint position, const bool insideViewport) {
    scene_.onPointerMove(position, insideViewport);
}

void Game::onClick(const SDL_FPoint position) {
    scene_.onPointerMove(position, true);
    if (inCutscene()) {
        // Cutscene: a click only skips the line being spoken.
        scene_.speech().hide();
        return;
    }
    if (!scene_.pointerInside()) {
        return;
    }
    if (const SceneObject* object = scene_.pick(position)) {
        (void)startObjectInteraction(InteractionTrigger::interact(object->id));
        return;
    }
    scene_.walkToFloor(position);
}

void Game::onTextInput(const std::string_view text) {
    scene_.speech().show(text);
}

void Game::tick(const float deltaSeconds) {
    const std::optional<InteractionTrigger> arrived = scene_.tick(deltaSeconds);
    if (script_.busy() || dialogue_.active()) {
        Host host(*this);
        script_.tick(deltaSeconds, host);
        settleScript();
    }
    if (arrived) {
        (void)resolveInteraction(*arrived);
    }
    scene_.followPlayer(deltaSeconds);
}

bool Game::inCutscene() const { return script_.busy() || dialogue_.active(); }

bool Game::dialogueActive() const { return dialogue_.active(); }

bool Game::awaitingDialogueChoice() const {
    return dialogue_.phase() == DialoguePhase::choosing;
}

std::span<const DialogueChoice* const> Game::dialogueOptions() const {
    return dialogue_.options();
}
