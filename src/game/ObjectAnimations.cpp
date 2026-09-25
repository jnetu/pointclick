#include "game/ObjectAnimations.hpp"

#include "game/Room.hpp"

#include <algorithm>
#include <utility>

void ObjectAnimations::rebuild(const Room& room, const ObjectAnimations* previous) {
    std::unordered_map<std::string, AnimationPlayback> next;
    for (const SceneObject& object : room.scenery) {
        if (object.animations.empty()) continue;
        AnimationPlayback playback(object.initialAnimation);
        if (previous) {
            if (const AnimationPlayback* old = previous->find(object.id);
                old && findAnimation(object.animations, old->name())) {
                playback = *old;
            }
        }
        next.emplace(object.id, std::move(playback));
    }
    active_ = std::move(next);
}

bool ObjectAnimations::play(const Room& room, const std::string_view objectId,
                            const std::string_view animation, const bool restart,
                            std::string& error) {
    const auto object = std::find_if(room.scenery.begin(), room.scenery.end(),
                                     [&](const SceneObject& item) { return item.id == objectId; });
    if (object == room.scenery.end()) { error = "Objeto desconhecido"; return false; }
    if (!findAnimation(object->animations, animation)) {
        error = "Animacao desconhecida para " + object->id;
        return false;
    }
    active_.at(object->id).play(animation, restart);
    error.clear();
    return true;
}

void ObjectAnimations::tick(const float deltaSeconds) {
    for (auto& [_, playback] : active_) playback.tick(deltaSeconds);
}

const AnimationPlayback* ObjectAnimations::find(const std::string_view objectId) const {
    const auto found = active_.find(std::string(objectId));
    return found == active_.end() ? nullptr : &found->second;
}
