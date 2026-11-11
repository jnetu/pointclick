#pragma once

#include "game/Facing.hpp"

#include <string_view>

class SessionState;

// What scripted actions may ask of the running scene. Game implements it; tests can fake it.
// Commands start something; queries let blocking actions poll until it is over.
class ScriptHost {
public:
    virtual ~ScriptHost() = default;

    virtual SessionState& session() = 0;
    [[nodiscard]] virtual std::string_view roomId() const = 0;
    // Session changes that affect collision; the host rebuilds navigation before walking.
    virtual void invalidateNavigation() = 0;

    virtual void say(std::string_view text) = 0;
    [[nodiscard]] virtual bool speaking() const = 0;

    virtual void playAnimation(std::string_view objectId, std::string_view animation) = 0;
    // True when the object's current clip ended, loops forever, or does not exist.
    [[nodiscard]] virtual bool animationFinished(std::string_view objectId) const = 0;

    // False when the player cannot move closer (already there, hidden or unreachable).
    virtual bool walkToObject(std::string_view objectId) = 0;
    [[nodiscard]] virtual bool walking() const = 0;
    // Turns the player in place; standing, it picks idle_<facing> when the set has it.
    virtual void facePlayer(Facing facing) = 0;

    // Opens a dialogue node; false when the room has no such node. The host runs the node's
    // steps on top of the current sequence once the calling action returns.
    virtual bool startDialogue(std::string_view nodeId) = 0;
    // True from startDialogue until the conversation ends (including while choosing).
    [[nodiscard]] virtual bool dialogueActive() const = 0;

    // Applied when the script ends, so later steps still run in the current room. spawnId names
    // a `[spawn]` of the target room; empty = its playerStart.
    virtual void changeRoom(std::string_view roomId, std::string_view spawnId) = 0;
};
