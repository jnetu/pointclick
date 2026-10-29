#pragma once

#include <SDL3/SDL_rect.h>

#include <optional>
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

struct InteractionTrigger {
    InteractionTriggerKind kind = InteractionTriggerKind::interact;
    std::string objectId;
    std::string itemId;

    [[nodiscard]] static InteractionTrigger interact(std::string objectId);
    [[nodiscard]] static InteractionTrigger useItem(std::string itemId, std::string objectId);
    [[nodiscard]] bool matches(const InteractionTrigger& other) const noexcept;
};

enum class InteractionConditionKind {
    flagSet,
    flagNotSet,
    hasItem,
    lacksItem,
    objectVisible,
    objectHidden,
};

struct InteractionCondition {
    InteractionConditionKind kind = InteractionConditionKind::flagSet;
    std::string targetId;

    [[nodiscard]] static InteractionCondition flagSet(std::string flag);
    [[nodiscard]] static InteractionCondition flagNotSet(std::string flag);
    [[nodiscard]] static InteractionCondition hasItem(std::string itemId);
    [[nodiscard]] static InteractionCondition lacksItem(std::string itemId);
    [[nodiscard]] static InteractionCondition objectVisible(std::string objectId);
    [[nodiscard]] static InteractionCondition objectHidden(std::string objectId);

    [[nodiscard]] bool evaluate(const SessionState& session, std::string_view roomId) const;
};

enum class InteractionEffectKind {
    say,
    setFlag,
    clearFlag,
    giveItem,
    takeItem,
    hideObject,
    showObject,
    setSolid,
    playAnimation,
    changeRoom,
};

struct InteractionEffect {
    InteractionEffectKind kind = InteractionEffectKind::say;
    std::string targetId;
    std::string value;
    bool boolValue = false;

    [[nodiscard]] static InteractionEffect say(std::string text);
    [[nodiscard]] static InteractionEffect setFlag(std::string flag);
    [[nodiscard]] static InteractionEffect clearFlag(std::string flag);
    [[nodiscard]] static InteractionEffect giveItem(std::string itemId);
    [[nodiscard]] static InteractionEffect takeItem(std::string itemId);
    [[nodiscard]] static InteractionEffect hideObject(std::string objectId);
    [[nodiscard]] static InteractionEffect showObject(std::string objectId);
    [[nodiscard]] static InteractionEffect setSolid(std::string objectId, bool solid);
    [[nodiscard]] static InteractionEffect playAnimation(std::string objectId,
                                                         std::string animation);
    [[nodiscard]] static InteractionEffect changeRoom(std::string roomId);
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

struct ObjectAnimationRequest {
    std::string objectId;
    std::string animation;
};

struct InteractionOutcome {
    bool matched = false;
    bool navigationChanged = false;
    std::optional<std::string> spokenText;
    std::vector<ObjectAnimationRequest> animations;
    std::optional<std::string> targetRoomId;
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

InteractionOutcome applyInteraction(const RoomInteractions& interactions,
                                    std::string_view roomId,
                                    const InteractionTrigger& trigger,
                                    SessionState& session);
