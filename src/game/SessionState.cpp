#include "game/SessionState.hpp"

#include "game/Identifier.hpp"
#include "game/SceneObject.hpp"

#include <algorithm>

void SessionState::reset() {
    flags_.clear();
    items_.clear();
    roomOverrides_.clear();
}

void SessionState::setFlag(const std::string_view id, const int value) {
    if (!validIdentifier(id)) return;
    if (value == 0) {
        clearFlag(id);
        return;
    }
    if (const auto found = flags_.find(id); found != flags_.end()) {
        found->second = value;
    } else {
        flags_.emplace(std::string(id), value);
    }
}

void SessionState::clearFlag(const std::string_view id) {
    if (const auto found = flags_.find(id); found != flags_.end()) {
        flags_.erase(found);
    }
}

bool SessionState::hasFlag(const std::string_view id) const {
    return flagValue(id) != 0;
}

int SessionState::flagValue(const std::string_view id) const {
    const auto found = flags_.find(id);
    return found == flags_.end() ? 0 : found->second;
}

int SessionState::incrementFlag(const std::string_view id, const int delta) {
    if (!validIdentifier(id)) return 0;
    const int next = flagValue(id) + delta;
    setFlag(id, next);
    return next;
}

bool SessionState::addItem(const std::string_view itemId) {
    if (!validIdentifier(itemId) || hasItem(itemId)) {
        return false;
    }
    items_.emplace_back(itemId);
    return true;
}

bool SessionState::removeItem(const std::string_view itemId) {
    const auto found = std::find(items_.begin(), items_.end(), itemId);
    if (found == items_.end()) {
        return false;
    }
    items_.erase(found);
    return true;
}

bool SessionState::hasItem(const std::string_view itemId) const {
    return std::find(items_.begin(), items_.end(), itemId) != items_.end();
}

const std::vector<std::string>& SessionState::items() const noexcept {
    return items_;
}

void SessionState::setObjectVisible(const std::string_view roomId,
                                    const std::string_view objectId,
                                    const bool visible) {
    if (!validIdentifier(roomId) || !validIdentifier(objectId)) return;
    roomOverrides_[std::string(roomId)][std::string(objectId)].visible = visible;
}

bool SessionState::isObjectVisible(const std::string_view roomId,
                                   const std::string_view objectId) const {
    const ObjectStateOverride* state = findOverride(roomId, objectId);
    return state && state->visible.has_value() ? *state->visible : true;
}

void SessionState::setObjectSolid(const std::string_view roomId,
                                  const std::string_view objectId,
                                  const bool solid) {
    if (!validIdentifier(roomId) || !validIdentifier(objectId)) return;
    roomOverrides_[std::string(roomId)][std::string(objectId)].solidOverride = solid;
}

void SessionState::clearObjectSolidOverride(const std::string_view roomId,
                                            const std::string_view objectId) {
    const auto roomFound = roomOverrides_.find(roomId);
    if (roomFound == roomOverrides_.end()) return;
    const auto objectFound = roomFound->second.find(objectId);
    if (objectFound == roomFound->second.end()) return;
    objectFound->second.solidOverride.reset();
}

std::optional<bool> SessionState::objectSolidOverride(const std::string_view roomId,
                                                      const std::string_view objectId) const {
    const ObjectStateOverride* state = findOverride(roomId, objectId);
    return state ? state->solidOverride : std::nullopt;
}

bool SessionState::isObjectSolid(const std::string_view roomId,
                                 const SceneObject& object) const {
    if (!isObjectVisible(roomId, object.id)) {
        return false;
    }
    if (const auto overrideValue = objectSolidOverride(roomId, object.id)) {
        return *overrideValue;
    }
    return object.type == SceneObjectType::solid;
}

const ObjectStateOverride* SessionState::findOverride(const std::string_view roomId,
                                                      const std::string_view objectId) const {
    const auto roomFound = roomOverrides_.find(roomId);
    if (roomFound == roomOverrides_.end()) return nullptr;
    const auto objectFound = roomFound->second.find(objectId);
    return objectFound == roomFound->second.end() ? nullptr : &objectFound->second;
}
