#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

struct Room;
class SessionState;

// Typed signatures for the declarative .rules vocabulary. Parser, serializer, validation,
// execution and documentation all read this catalog: a new effect or condition is one entry
// plus one function. Mirrors the RoomParameters model for non-numeric data.
enum class ArgType : std::uint8_t {
    objectId,  // validated against the room when one is given
    itemId,
    flagId,
    roomId,
    animation, // name only; cross-checks against an object belong in the entry's validate
    text,      // quoted string with \" \\ \n escapes, must not be empty
    boolean,   // stored canonically as "0" or "1"
};

[[nodiscard]] std::string_view argPlaceholder(ArgType type);

struct ArgSignature {
    static constexpr std::size_t capacity = 4;

    std::array<ArgType, capacity> types{};
    std::size_t count = 0;

    constexpr ArgSignature(const std::initializer_list<ArgType> list) {
        for (const ArgType type : list) {
            types[count++] = type; // Over capacity fails constant evaluation of the catalog.
        }
    }

    [[nodiscard]] constexpr std::span<const ArgType> view() const { return {types.data(), count}; }
};

using ActionArgs = std::span<const std::string>;

// Outputs consumed by runtime systems outside SessionState. Effects that only touch the
// session emit nothing; a new kind of output adds one alternative here and one handler in Game.
struct SayEvent {
    std::string text;
};

struct PlayAnimationEvent {
    std::string objectId;
    std::string animation;
};

struct ChangeRoomEvent {
    std::string roomId;
};

using InteractionEvent = std::variant<SayEvent, PlayAnimationEvent, ChangeRoomEvent>;

struct EffectContext {
    SessionState& session;
    std::string_view roomId;
    std::vector<InteractionEvent>& events;
    bool navigationChanged = false;
};

struct EffectDescriptor {
    std::string_view keyword;
    std::string_view description;
    ArgSignature args;
    // Optional extra check after the per-ArgType one (args.size() already matches). Empty = ok.
    std::string (*validate)(ActionArgs args, const Room* room);
    void (*run)(ActionArgs args, EffectContext& context);
};

struct ConditionDescriptor {
    std::string_view keyword;
    std::string_view description;
    ArgSignature args;
    bool (*evaluate)(ActionArgs args, const SessionState& session, std::string_view roomId);
};

[[nodiscard]] std::span<const EffectDescriptor> effectCatalog();
[[nodiscard]] std::span<const ConditionDescriptor> conditionCatalog();
[[nodiscard]] const EffectDescriptor* findEffect(std::string_view keyword);
[[nodiscard]] const ConditionDescriptor* findCondition(std::string_view keyword);

// "Use set_solid <objeto> <0|1>", generated from the signature.
[[nodiscard]] std::string actionUsage(std::string_view keyword, const ArgSignature& signature);

// Shared by the file parser (with line numbers) and rule validation. Empty means valid.
[[nodiscard]] std::string argumentsError(std::string_view keyword, const ArgSignature& signature,
                                         ActionArgs args, const Room* room);
[[nodiscard]] std::string effectArgumentsError(const EffectDescriptor& effect, ActionArgs args,
                                               const Room* room);
