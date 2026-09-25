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

    [[nodiscard]] std::string_view name() const;
    [[nodiscard]] float elapsed() const;
    [[nodiscard]] bool finished(const SpriteClip& clip) const;

private:
    std::string name_;
    float elapsed_ = 0.0F;
};
