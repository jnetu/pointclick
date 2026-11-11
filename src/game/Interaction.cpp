#include "game/Interaction.hpp"

#include "game/Identifier.hpp"
#include "game/Navigation.hpp"
#include "game/Room.hpp"
#include "game/SessionState.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {
constexpr float kMaxInteractionReachPixels = static_cast<float>(Navigation::cellSize) * 3.0F;

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

bool InteractionTrigger::matches(const InteractionTrigger& event) const noexcept {
    if (kind != event.kind || (objectId != wildcard && objectId != event.objectId)) {
        return false;
    }
    return kind == InteractionTriggerKind::interact || itemId == wildcard
           || itemId == event.itemId;
}

int InteractionTrigger::wildcardLevel() const noexcept {
    const bool anyObject = objectId == wildcard;
    if (kind == InteractionTriggerKind::interact) return anyObject ? 1 : 0;
    const bool anyItem = itemId == wildcard;
    if (anyObject && anyItem) return 3; // [use_item * *]
    if (anyObject) return 2;            // [use_item item *]
    return anyItem ? 1 : 0;             // [use_item * obj] or exact
}

InteractionCondition InteractionCondition::make(const std::string_view keyword,
                                                std::vector<std::string> args) {
    const ConditionDescriptor* condition = findCondition(keyword);
    if (!condition) {
        throw std::invalid_argument("Condicao desconhecida: " + std::string(keyword));
    }
    return {condition, std::move(args)};
}

bool InteractionCondition::evaluate(const SessionState& session,
                                    const std::string_view roomId) const {
    return condition && condition->evaluate(args, session, roomId);
}

std::string InteractionCondition::validationError(const Room* room) const {
    if (!condition) return "Condicao sem tipo";
    return argumentsError(condition->keyword, condition->args, args, room);
}

InteractionEffect InteractionEffect::make(const std::string_view keyword,
                                          std::vector<std::string> args) {
    const EffectDescriptor* effect = findEffect(keyword);
    if (!effect) {
        throw std::invalid_argument("Efeito desconhecido: " + std::string(keyword));
    }
    return {effect, std::move(args)};
}

std::string InteractionEffect::validationError(const Room* room) const {
    if (!effect) return "Efeito sem tipo";
    return effectArgumentsError(*effect, args, room);
}

std::string InteractionRule::validationError(const Room* room) const {
    const bool anyObject = trigger.objectId == InteractionTrigger::wildcard;
    if (!anyObject && !validIdentifier(trigger.objectId)) {
        return "Identificador de objeto invalido no gatilho";
    }
    if (trigger.kind == InteractionTriggerKind::useItem && trigger.itemId != InteractionTrigger::wildcard
        && !validIdentifier(trigger.itemId)) {
        return "Identificador de item invalido no gatilho";
    }
    if (trigger.kind == InteractionTriggerKind::interact && !trigger.itemId.empty()) {
        return "Gatilho interact nao aceita item";
    }
    if (room && !anyObject && !room->findObject(trigger.objectId)) {
        return "Objeto desconhecido na sala: " + trigger.objectId;
    }
    for (const InteractionCondition& condition : conditions) {
        if (std::string error = condition.validationError(room); !error.empty()) return error;
    }
    if (effects.empty()) {
        return "Regra sem efeitos";
    }
    for (const InteractionEffect& effect : effects) {
        if (std::string error = effect.validationError(room); !error.empty()) return error;
    }
    return {};
}

bool RoomInteractions::hasRulesForObject(const std::string_view objectId) const {
    return std::any_of(rules.begin(), rules.end(), [&](const InteractionRule& rule) {
        return rule.trigger.objectId == objectId
               || rule.trigger.objectId == InteractionTrigger::wildcard;
    });
}

const InteractionRule* RoomInteractions::findMatchingRule(const InteractionTrigger& trigger,
                                                          const SessionState& session,
                                                          const std::string_view roomId) const {
    // The most specific level wins regardless of file order; within a level, the first rule
    // whose conditions hold. A specific rule whose conditions fail falls through to wildcards.
    for (int level = 0; level <= InteractionTrigger::maxWildcardLevel; ++level) {
        for (const InteractionRule& rule : rules) {
            if (rule.trigger.wildcardLevel() != level || !rule.trigger.matches(trigger)) {
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
