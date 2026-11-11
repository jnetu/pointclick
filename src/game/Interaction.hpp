#pragma once

#include "game/ActionCatalog.hpp"

#include <SDL3/SDL_rect.h>

#include <string>
#include <string_view>
#include <vector>

struct Room;
struct SceneObject;
class SessionState;

enum class InteractionTriggerKind {
    interact,
    useItem,
};

// A rule's objectId or itemId may be `wildcard` ("*", never a valid identifier): `[interact *]`,
// `[use_item * obj]`, `[use_item item *]`, `[use_item * *]`. Events are always concrete.
struct InteractionTrigger {
    static constexpr std::string_view wildcard = "*";

    InteractionTriggerKind kind = InteractionTriggerKind::interact;
    std::string objectId;
    std::string itemId;

    [[nodiscard]] static InteractionTrigger interact(std::string objectId);
    [[nodiscard]] static InteractionTrigger useItem(std::string itemId, std::string objectId);
    // this = a rule's trigger (wildcards allowed), event = what the player did.
    [[nodiscard]] bool matches(const InteractionTrigger& event) const noexcept;
    // 0 = exact; higher = broader. findMatchingRule tries every rule of a level before the next:
    // exact, then `* obj` (the object's own fallback), then `item *`, then `* *`.
    [[nodiscard]] int wildcardLevel() const noexcept;
    static constexpr int maxWildcardLevel = 3;
};

// A condition or effect is one catalog entry (game/ActionCatalog) plus its typed arguments,
// stored as text in signature order. Adding vocabulary never touches these structs.
struct InteractionCondition {
    const ConditionDescriptor* condition = nullptr;
    std::vector<std::string> args;

    // Throws std::invalid_argument for unknown keywords (authoring bug in C++ presets/tests).
    [[nodiscard]] static InteractionCondition make(std::string_view keyword,
                                                   std::vector<std::string> args);
    [[nodiscard]] bool evaluate(const SessionState& session, std::string_view roomId) const;
    [[nodiscard]] std::string validationError(const Room* room) const;
};

struct InteractionEffect {
    const EffectDescriptor* effect = nullptr;
    std::vector<std::string> args;

    // Throws std::invalid_argument for unknown keywords (authoring bug in C++ presets/tests).
    [[nodiscard]] static InteractionEffect make(std::string_view keyword,
                                                std::vector<std::string> args);
    [[nodiscard]] std::string validationError(const Room* room) const;
};

struct InteractionRule {
    InteractionTrigger trigger;
    std::vector<InteractionCondition> conditions;
    std::vector<InteractionEffect> effects;

    [[nodiscard]] std::string validationError(const Room* room = nullptr) const;
};

struct RoomInteractions {
    std::vector<InteractionRule> rules;

    [[nodiscard]] bool empty() const noexcept { return rules.empty(); }
    // Counts wildcard triggers too: `[interact *]` makes every visible object clickable.
    [[nodiscard]] bool hasRulesForObject(std::string_view objectId) const;
    [[nodiscard]] const InteractionRule* findMatchingRule(const InteractionTrigger& trigger,
                                                          const SessionState& session,
                                                          std::string_view roomId) const;
    [[nodiscard]] std::string validationError(const Room* room = nullptr) const;
};

struct PendingInteraction {
    std::string objectId;
    InteractionTriggerKind kind = InteractionTriggerKind::interact;
    std::string itemId;

    [[nodiscard]] InteractionTrigger toTrigger() const {
        return {kind, objectId, itemId};
    }
};

[[nodiscard]] SDL_FRect effectiveCollisionFootprint(const SceneObject& object);

// Picks the front-most visible interactive object at worldPoint, matching the
// exact inverse depth order of SceneRenderer's stable Y-sort.
[[nodiscard]] const SceneObject* pickInteractiveObject(const Room& room,
                                                       const SessionState& session,
                                                       const RoomInteractions& interactions,
                                                       SDL_FPoint worldPoint);

[[nodiscard]] SDL_FPoint interactionApproachPoint(const Room& room,
                                                  const SceneObject& object,
                                                  const SessionState& session);

[[nodiscard]] bool isWithinInteractionReach(const Room& room,
                                            const SceneObject& object,
                                            const SessionState& session,
                                            SDL_FPoint feet);
