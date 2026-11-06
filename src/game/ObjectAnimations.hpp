#pragma once

#include "game/Animation.hpp"
#include "game/StringMap.hpp"

#include <string_view>
#include <unordered_map>

struct Room;

// Runtime animation state for objects identified by stable room IDs.
class ObjectAnimations {
public:
    void rebuild(const Room& room, const ObjectAnimations* previous = nullptr);
    bool play(const Room& room, std::string_view objectId,
              std::string_view animation, bool restart, std::string& error);
    void tick(float deltaSeconds);
    [[nodiscard]] const AnimationPlayback* find(std::string_view objectId) const;

private:
    StringMap<AnimationPlayback> active_;
};
