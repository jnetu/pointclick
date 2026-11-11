#pragma once

#include "game/Animation.hpp"
#include "game/StringMap.hpp"

#include <string_view>
#include <unordered_map>

struct Room;
class SessionState;

// Runtime animation state for objects identified by stable room IDs.
class ObjectAnimations {
public:
    // Each object starts, in this order: where `previous` had it (live edit of the same room);
    // at the clip the session saved for it (set_animation), already on its last frame when the
    // clip does not repeat, so a door loaded open is not seen opening again; at its
    // initialAnimation. A saved clip the object no longer has is ignored.
    void rebuild(const Room& room, const SessionState& session,
                 const ObjectAnimations* previous = nullptr);
    bool play(const Room& room, std::string_view objectId,
              std::string_view animation, bool restart, std::string& error);
    void tick(float deltaSeconds);
    [[nodiscard]] const AnimationPlayback* find(std::string_view objectId) const;

private:
    StringMap<AnimationPlayback> active_;
};
