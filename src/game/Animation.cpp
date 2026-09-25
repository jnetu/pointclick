#include "game/Animation.hpp"

#include <algorithm>
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
    elapsed_ = 0.0F;
    return true;
}

void AnimationPlayback::tick(const float deltaSeconds) {
    elapsed_ += std::max(deltaSeconds, 0.0F);
}

std::string_view AnimationPlayback::name() const { return name_; }

float AnimationPlayback::elapsed() const { return elapsed_; }

bool AnimationPlayback::finished(const SpriteClip& clip) const {
    return !clip.loop && clip.fps > 0.0F
           && elapsed_ * clip.fps >= static_cast<float>(clip.frames);
}
