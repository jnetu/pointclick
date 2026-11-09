#include "game/Animation.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

const SpriteClip* findAnimation(const AnimationSet& animations,
                                const std::string_view name) {
    const auto found = animations.find(name);
    return found == animations.end() ? nullptr : &found->second;
}

AnimationPlayback::AnimationPlayback(std::string name) : name_(std::move(name)) {}

bool AnimationPlayback::play(const std::string_view name, const bool restart) {
    if (!restart && name_ == name) return false;
    name_ = name;
    previousElapsed_ = 0.0F;
    elapsed_ = 0.0F;
    return true;
}

void AnimationPlayback::tick(const float deltaSeconds) {
    previousElapsed_ = elapsed_;
    elapsed_ += std::max(deltaSeconds, 0.0F);
}

std::string_view AnimationPlayback::name() const { return name_; }

float AnimationPlayback::elapsed() const { return elapsed_; }

float AnimationPlayback::interpolatedElapsed(const float alpha) const {
    return std::lerp(previousElapsed_, elapsed_, std::clamp(alpha, 0.0F, 1.0F));
}

bool AnimationPlayback::finished(const SpriteClip& clip) const {
    if (clip.loop) return false;
    // fps 0 holds the first frame: nothing is left to play, so a script never waits on it.
    return clip.fps <= 0.0F || elapsed_ * clip.fps >= static_cast<float>(clip.frames);
}
