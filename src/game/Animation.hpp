#pragma once

#include "game/SpriteClip.hpp"
#include "game/Identifier.hpp"

#include <map>
#include <string>
#include <string_view>

// Definitions belong to the room. Names are stable keys for gameplay rules.
using AnimationSet = std::map<std::string, SpriteClip, std::less<>>;

[[nodiscard]] const SpriteClip* findAnimation(const AnimationSet& animations,
                                              std::string_view name);

// Playback belongs to one entity. Rendering only reads this state.
class AnimationPlayback {
public:
    explicit AnimationPlayback(std::string name = "idle");

    // Returns true when the active animation changed or was restarted.
    bool play(std::string_view name, bool restart = false);
    void tick(float deltaSeconds);
    // Jumps to the end of a clip that does not repeat (last frame, finished()), as when a room
    // is entered with the object already in that state. A looping clip is left at its start.
    void finish(const SpriteClip& clip);

    [[nodiscard]] std::string_view name() const;
    [[nodiscard]] float elapsed() const;
    // Render time between the last two ticks (alpha in [0,1]); restarts never blend backwards.
    [[nodiscard]] float interpolatedElapsed(float alpha) const;
    [[nodiscard]] bool finished(const SpriteClip& clip) const;

private:
    std::string name_;
    float previousElapsed_ = 0.0F;
    float elapsed_ = 0.0F;
};
