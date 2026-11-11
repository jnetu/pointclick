#include "game/SessionState.hpp"

#include "game/Identifier.hpp"
#include "game/SceneObject.hpp"

#include <algorithm>
#include <cstdint>
#include <set>
#include <tuple>
#include <utility>

void SessionState::reset() {
    flags_.clear();
    items_.clear();
    roomOverrides_.clear();
    dialogueChoices_.clear();
}

std::string SessionSnapshot::validationError() const {
    std::set<std::string_view> flagIds;
    for (const Flag& flag : flags) {
        if (!validIdentifier(flag.id)) return "Flag invalida: " + flag.id;
        if (flag.value == 0) return "Flag com valor 0 (ausente): " + flag.id;
        if (flag.value < -kMaxFlagValue || flag.value > kMaxFlagValue) {
            return "Flag fora do limite de +-" + std::to_string(kMaxFlagValue) + ": " + flag.id
                   + " = " + std::to_string(flag.value);
        }
        if (!flagIds.insert(flag.id).second) return "Flag repetida: " + flag.id;
    }
    std::set<std::string_view> itemIds;
    for (const std::string& item : items) {
        if (!validIdentifier(item)) return "Item invalido: " + item;
        if (!itemIds.insert(item).second) return "Item repetido: " + item;
    }
    std::set<std::pair<std::string_view, std::string_view>> objectIds;
    for (const ObjectOverride& object : objects) {
        const std::string name = object.roomId + "." + object.objectId;
        if (!validIdentifier(object.roomId) || !validIdentifier(object.objectId)) {
            return "Objeto invalido: " + name;
        }
        if (!object.visible && !object.solidOverride && !object.animation) {
            return "Objeto sem estado: " + name;
        }
        if (object.animation && !validIdentifier(*object.animation)) {
            return "Animacao invalida em " + name + ": " + *object.animation;
        }
        if (!objectIds.emplace(object.roomId, object.objectId).second) {
            return "Objeto repetido: " + name;
        }
    }
    std::set<std::tuple<std::string_view, std::string_view, std::string_view>> choiceIds;
    for (const DialogueChoice& choice : dialogueChoices) {
        const std::string name = choice.roomId + "/" + choice.nodeId + "/" + choice.choiceId;
        if (!validIdentifier(choice.roomId) || !validIdentifier(choice.nodeId)
            || !validIdentifier(choice.choiceId)) {
            return "Escolha de dialogo invalida: " + name;
        }
        if (!choiceIds.emplace(choice.roomId, choice.nodeId, choice.choiceId).second) {
            return "Escolha de dialogo repetida: " + name;
        }
    }
    return {};
}

SessionSnapshot SessionState::snapshot() const {
    SessionSnapshot out;
    out.flags.reserve(flags_.size());
    for (const auto& [id, value] : flags_) out.flags.push_back({id, value});
    std::sort(out.flags.begin(), out.flags.end(),
              [](const auto& a, const auto& b) { return a.id < b.id; });
    out.items = items_;
    for (const auto& [roomId, objects] : roomOverrides_) {
        for (const auto& [objectId, state] : objects) {
            if (!state.visible && !state.solidOverride && !state.animation) continue; // cleared
            out.objects.push_back(
                    {roomId, objectId, state.visible, state.solidOverride, state.animation});
        }
    }
    std::sort(out.objects.begin(), out.objects.end(), [](const auto& a, const auto& b) {
        return std::tie(a.roomId, a.objectId) < std::tie(b.roomId, b.objectId);
    });
    out.dialogueChoices.reserve(dialogueChoices_.size());
    for (const std::string& key : dialogueChoices_) {
        // "room/node/choice": identifiers never contain '/'.
        const auto first = key.find('/');
        const auto second = key.find('/', first + 1);
        out.dialogueChoices.push_back({key.substr(0, first),
                                       key.substr(first + 1, second - first - 1),
                                       key.substr(second + 1)});
    }
    std::sort(out.dialogueChoices.begin(), out.dialogueChoices.end(),
              [](const auto& a, const auto& b) {
                  return std::tie(a.roomId, a.nodeId, a.choiceId)
                         < std::tie(b.roomId, b.nodeId, b.choiceId);
              });
    return out;
}

bool SessionState::restore(const SessionSnapshot& snapshot, std::string& error) {
    error = snapshot.validationError();
    if (!error.empty()) return false;
    SessionState next;
    for (const auto& flag : snapshot.flags) next.setFlag(flag.id, flag.value);
    for (const auto& item : snapshot.items) next.addItem(item);
    for (const auto& object : snapshot.objects) {
        if (object.visible) next.setObjectVisible(object.roomId, object.objectId, *object.visible);
        if (object.solidOverride) {
            next.setObjectSolid(object.roomId, object.objectId, *object.solidOverride);
        }
        if (object.animation) {
            next.setObjectAnimation(object.roomId, object.objectId, *object.animation);
        }
    }
    for (const auto& choice : snapshot.dialogueChoices) {
        next.markDialogueChoice(choice.roomId, choice.nodeId, choice.choiceId);
    }
    *this = std::move(next);
    return true;
}

void SessionState::setFlag(const std::string_view id, const int value) {
    if (!validIdentifier(id)) return;
    if (value == 0) {
        clearFlag(id);
        return;
    }
    const int stored = std::clamp(value, -kMaxFlagValue, kMaxFlagValue);
    if (const auto found = flags_.find(id); found != flags_.end()) {
        found->second = stored;
    } else {
        flags_.emplace(std::string(id), stored);
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
    // 64 bits: any int + int fits, then it saturates at the limit instead of overflowing.
    const std::int64_t sum = std::int64_t{flagValue(id)} + delta;
    const int next = static_cast<int>(
            std::clamp<std::int64_t>(sum, -kMaxFlagValue, kMaxFlagValue));
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

void SessionState::setObjectAnimation(const std::string_view roomId,
                                      const std::string_view objectId,
                                      const std::string_view animation) {
    if (!validIdentifier(roomId) || !validIdentifier(objectId) || !validIdentifier(animation)) {
        return;
    }
    roomOverrides_[std::string(roomId)][std::string(objectId)].animation = std::string(animation);
}

std::string_view SessionState::objectAnimation(const std::string_view roomId,
                                               const std::string_view objectId) const {
    const ObjectStateOverride* state = findOverride(roomId, objectId);
    return state && state->animation ? std::string_view(*state->animation) : std::string_view();
}

namespace {
std::string dialogueChoiceKey(const std::string_view roomId, const std::string_view nodeId,
                              const std::string_view choiceId) {
    std::string key;
    key.reserve(roomId.size() + nodeId.size() + choiceId.size() + 2);
    key.append(roomId).append(1, '/').append(nodeId).append(1, '/').append(choiceId);
    return key;
}
} // namespace

void SessionState::markDialogueChoice(const std::string_view roomId,
                                      const std::string_view nodeId,
                                      const std::string_view choiceId) {
    if (!validIdentifier(roomId) || !validIdentifier(nodeId) || !validIdentifier(choiceId)) return;
    dialogueChoices_.insert(dialogueChoiceKey(roomId, nodeId, choiceId));
}

bool SessionState::dialogueChoiceTaken(const std::string_view roomId,
                                       const std::string_view nodeId,
                                       const std::string_view choiceId) const {
    return dialogueChoices_.contains(dialogueChoiceKey(roomId, nodeId, choiceId));
}

const ObjectStateOverride* SessionState::findOverride(const std::string_view roomId,
                                                      const std::string_view objectId) const {
    const auto roomFound = roomOverrides_.find(roomId);
    if (roomFound == roomOverrides_.end()) return nullptr;
    const auto objectFound = roomFound->second.find(objectId);
    return objectFound == roomFound->second.end() ? nullptr : &objectFound->second;
}
