#include "game/Interaction.hpp"

#include "game/Identifier.hpp"
#include "game/Navigation.hpp"
#include "game/Room.hpp"
#include "game/SessionState.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace {
constexpr float kMaxInteractionReachPixels = static_cast<float>(Navigation::cellSize) * 3.0F;

const SceneObject* findSceneObject(const Room& room, const std::string_view objectId) {
    const auto found = std::find_if(room.scenery.begin(), room.scenery.end(),
                                    [&](const SceneObject& obj) { return obj.id == objectId; });
    return found == room.scenery.end() ? nullptr : &(*found);
}

SDL_FRect paddedObstacleRect(const Room& room, const SceneObject& object) {
    const SDL_FRect base = effectiveCollisionFootprint(object);
    return {base.x - room.collisionPaddingX,
            base.y - room.collisionPaddingY,
            base.w + room.collisionPaddingX * 2.0F,
            base.h + room.collisionPaddingY * 2.0F};
}

float distanceToRect(const SDL_FPoint point, const SDL_FRect& rect) {
    const float dx = std::max({rect.x - point.x, 0.0F, point.x - (rect.x + rect.w)});
    const float dy = std::max({rect.y - point.y, 0.0F, point.y - (rect.y + rect.h)});
    return std::hypot(dx, dy);
}
} // namespace

InteractionTrigger InteractionTrigger::interact(std::string objectId) {
    return {InteractionTriggerKind::interact, std::move(objectId), {}};
}

InteractionTrigger InteractionTrigger::useItem(std::string itemId, std::string objectId) {
    return {InteractionTriggerKind::useItem, std::move(objectId), std::move(itemId)};
}

bool InteractionTrigger::matches(const InteractionTrigger& other) const noexcept {
    if (kind != other.kind || objectId != other.objectId) {
        return false;
    }
    return kind == InteractionTriggerKind::interact || itemId == other.itemId;
}

InteractionCondition InteractionCondition::flagSet(std::string flag) {
    return {InteractionConditionKind::flagSet, std::move(flag)};
}

InteractionCondition InteractionCondition::flagNotSet(std::string flag) {
    return {InteractionConditionKind::flagNotSet, std::move(flag)};
}

InteractionCondition InteractionCondition::hasItem(std::string itemId) {
    return {InteractionConditionKind::hasItem, std::move(itemId)};
}

InteractionCondition InteractionCondition::lacksItem(std::string itemId) {
    return {InteractionConditionKind::lacksItem, std::move(itemId)};
}

InteractionCondition InteractionCondition::objectVisible(std::string objectId) {
    return {InteractionConditionKind::objectVisible, std::move(objectId)};
}

InteractionCondition InteractionCondition::objectHidden(std::string objectId) {
    return {InteractionConditionKind::objectHidden, std::move(objectId)};
}

bool InteractionCondition::evaluate(const SessionState& session,
                                    const std::string_view roomId) const {
    switch (kind) {
    case InteractionConditionKind::flagSet:
        return session.hasFlag(targetId);
    case InteractionConditionKind::flagNotSet:
        return !session.hasFlag(targetId);
    case InteractionConditionKind::hasItem:
        return session.hasItem(targetId);
    case InteractionConditionKind::lacksItem:
        return !session.hasItem(targetId);
    case InteractionConditionKind::objectVisible:
        return session.isObjectVisible(roomId, targetId);
    case InteractionConditionKind::objectHidden:
        return !session.isObjectVisible(roomId, targetId);
    }
    return false;
}

InteractionEffect InteractionEffect::say(std::string text) {
    return {InteractionEffectKind::say, {}, std::move(text), false};
}

InteractionEffect InteractionEffect::setFlag(std::string flag) {
    return {InteractionEffectKind::setFlag, std::move(flag), {}, false};
}

InteractionEffect InteractionEffect::clearFlag(std::string flag) {
    return {InteractionEffectKind::clearFlag, std::move(flag), {}, false};
}

InteractionEffect InteractionEffect::giveItem(std::string itemId) {
    return {InteractionEffectKind::giveItem, std::move(itemId), {}, false};
}

InteractionEffect InteractionEffect::takeItem(std::string itemId) {
    return {InteractionEffectKind::takeItem, std::move(itemId), {}, false};
}

InteractionEffect InteractionEffect::hideObject(std::string objectId) {
    return {InteractionEffectKind::hideObject, std::move(objectId), {}, false};
}

InteractionEffect InteractionEffect::showObject(std::string objectId) {
    return {InteractionEffectKind::showObject, std::move(objectId), {}, false};
}

InteractionEffect InteractionEffect::setSolid(std::string objectId, const bool solid) {
    return {InteractionEffectKind::setSolid, std::move(objectId), {}, solid};
}

InteractionEffect InteractionEffect::playAnimation(std::string objectId, std::string animation) {
    return {InteractionEffectKind::playAnimation, std::move(objectId), std::move(animation), false};
}

InteractionEffect InteractionEffect::changeRoom(std::string roomId) {
    return {InteractionEffectKind::changeRoom, std::move(roomId), {}, false};
}

std::string InteractionRule::validationError(const Room* room) const {
    if (!validIdentifier(trigger.objectId)) {
        return "Identificador de objeto invalido no gatilho";
    }
    if (trigger.kind == InteractionTriggerKind::useItem && !validIdentifier(trigger.itemId)) {
        return "Identificador de item invalido no gatilho";
    }
    if (trigger.kind == InteractionTriggerKind::interact && !trigger.itemId.empty()) {
        return "Gatilho interact nao aceita item";
    }
    if (room && !findSceneObject(*room, trigger.objectId)) {
        return "Objeto desconhecido na sala: " + trigger.objectId;
    }

    for (const InteractionCondition& condition : conditions) {
        if (!validIdentifier(condition.targetId)) {
            return "Identificador invalido na condicao";
        }
        if (room && (condition.kind == InteractionConditionKind::objectVisible
                     || condition.kind == InteractionConditionKind::objectHidden)
            && !findSceneObject(*room, condition.targetId)) {
            return "Objeto desconhecido na sala: " + condition.targetId;
        }
    }

    if (effects.empty()) {
        return "Regra sem efeitos";
    }

    for (const InteractionEffect& effect : effects) {
        switch (effect.kind) {
        case InteractionEffectKind::say:
            if (effect.value.empty()) {
                return "Texto de fala vazio";
            }
            break;
        case InteractionEffectKind::setFlag:
        case InteractionEffectKind::clearFlag:
        case InteractionEffectKind::giveItem:
        case InteractionEffectKind::takeItem:
        case InteractionEffectKind::changeRoom:
            if (!validIdentifier(effect.targetId)) {
                return "Identificador invalido no efeito";
            }
            break;
        case InteractionEffectKind::hideObject:
        case InteractionEffectKind::showObject:
        case InteractionEffectKind::setSolid:
            if (!validIdentifier(effect.targetId)) {
                return "Identificador de objeto invalido no efeito";
            }
            if (room && !findSceneObject(*room, effect.targetId)) {
                return "Objeto desconhecido na sala: " + effect.targetId;
            }
            break;
        case InteractionEffectKind::playAnimation:
            if (!validIdentifier(effect.targetId) || !validIdentifier(effect.value)) {
                return "Identificador invalido em play_animation";
            }
            if (room) {
                const SceneObject* target = findSceneObject(*room, effect.targetId);
                if (!target) {
                    return "Objeto desconhecido na sala: " + effect.targetId;
                }
                if (!findAnimation(target->animations, effect.value)) {
                    return "Animacao desconhecida '" + effect.value + "' no objeto: " + effect.targetId;
                }
            }
            break;
        }
    }
    return {};
}

bool RoomInteractions::hasRulesForObject(const std::string_view objectId) const {
    return std::any_of(rules.begin(), rules.end(), [&](const InteractionRule& rule) {
        return rule.trigger.objectId == objectId;
    });
}

const InteractionRule* RoomInteractions::findMatchingRule(const InteractionTrigger& trigger,
                                                          const SessionState& session,
                                                          const std::string_view roomId) const {
    for (const InteractionRule& rule : rules) {
        if (!rule.trigger.matches(trigger)) {
            continue;
        }
        const bool allConditionsMet = std::all_of(
                rule.conditions.begin(), rule.conditions.end(),
                [&](const InteractionCondition& condition) {
                    return condition.evaluate(session, roomId);
                });
        if (allConditionsMet) {
            return &rule;
        }
    }
    return nullptr;
}

std::string RoomInteractions::validationError(const Room* room) const {
    for (const InteractionRule& rule : rules) {
        if (const std::string error = rule.validationError(room); !error.empty()) {
            return error;
        }
    }
    return {};
}

SDL_FRect effectiveCollisionFootprint(const SceneObject& object) {
    if (object.collisionFootprint.w > 0.0F && object.collisionFootprint.h > 0.0F) {
        return object.worldCollisionFootprint();
    }
    return object.bounds;
}

const SceneObject* pickInteractiveObject(const Room& room,
                                         const SessionState& session,
                                         const RoomInteractions& interactions,
                                         const SDL_FPoint worldPoint) {
    const SceneObject* best = nullptr;
    float bestDepth = -std::numeric_limits<float>::infinity();
    for (const SceneObject& object : room.scenery) {
        if (!session.isObjectVisible(room.id, object.id)) continue;
        if (!interactions.hasRulesForObject(object.id)) continue;
        if (!SDL_PointInRectFloat(&worldPoint, &object.bounds)) continue;
        // SceneRenderer uses std::stable_sort by ascending depth(); later equal-depth
        // entries draw on top, so >= picks in exact inverse visual order.
        if (const float depth = object.depth(); depth >= bestDepth) {
            bestDepth = depth;
            best = &object;
        }
    }
    return best;
}

SDL_FPoint interactionApproachPoint(const Room& room,
                                    const SceneObject& object,
                                    const SessionState& session) {
    if (session.isObjectSolid(room.id, object)) {
        const SDL_FRect base = effectiveCollisionFootprint(object);
        return room.clampDestination({base.x + base.w * 0.5F, base.y + base.h});
    }
    return room.clampDestination({object.bounds.x + object.bounds.w * 0.5F,
                                  object.bounds.y + object.bounds.h});
}

bool isWithinInteractionReach(const Room& room,
                              const SceneObject& object,
                              const SessionState& session,
                              const SDL_FPoint feet) {
    const SDL_FPoint desired = interactionApproachPoint(room, object, session);
    if (std::hypot(feet.x - desired.x, feet.y - desired.y) <= kMaxInteractionReachPixels) {
        return true;
    }
    if (session.isObjectSolid(room.id, object)) {
        return distanceToRect(feet, paddedObstacleRect(room, object)) <= kMaxInteractionReachPixels;
    }
    // Support decorative pickups placed on top of a solid surface (e.g. a key on a table).
    for (const SceneObject& other : room.scenery) {
        if (!session.isObjectSolid(room.id, other)) continue;
        const SDL_FRect padded = paddedObstacleRect(room, other);
        if (SDL_PointInRectFloat(&desired, &padded)
            && distanceToRect(feet, padded) <= kMaxInteractionReachPixels) {
            return true;
        }
    }
    return false;
}

InteractionOutcome applyInteraction(const RoomInteractions& interactions,
                                    const std::string_view roomId,
                                    const InteractionTrigger& trigger,
                                    SessionState& session) {
    InteractionOutcome outcome;
    const InteractionRule* rule = interactions.findMatchingRule(trigger, session, roomId);
    if (!rule) {
        return outcome;
    }

    outcome.matched = true;
    for (const InteractionEffect& effect : rule->effects) {
        switch (effect.kind) {
        case InteractionEffectKind::say:
            if (!outcome.spokenText) {
                outcome.spokenText = effect.value;
            } else {
                *outcome.spokenText += "\n" + effect.value;
            }
            break;
        case InteractionEffectKind::setFlag:
            session.setFlag(effect.targetId);
            break;
        case InteractionEffectKind::clearFlag:
            session.clearFlag(effect.targetId);
            break;
        case InteractionEffectKind::giveItem:
            session.addItem(effect.targetId);
            break;
        case InteractionEffectKind::takeItem:
            session.removeItem(effect.targetId);
            break;
        case InteractionEffectKind::hideObject:
            session.setObjectVisible(roomId, effect.targetId, false);
            outcome.navigationChanged = true;
            break;
        case InteractionEffectKind::showObject:
            session.setObjectVisible(roomId, effect.targetId, true);
            outcome.navigationChanged = true;
            break;
        case InteractionEffectKind::setSolid:
            session.setObjectSolid(roomId, effect.targetId, effect.boolValue);
            outcome.navigationChanged = true;
            break;
        case InteractionEffectKind::playAnimation:
            outcome.animations.push_back({effect.targetId, effect.value});
            break;
        case InteractionEffectKind::changeRoom:
            outcome.targetRoomId = effect.targetId;
            break;
        }
    }
    return outcome;
}
