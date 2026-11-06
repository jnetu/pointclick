#pragma once

#include "game/ScriptHost.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>

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
    number,    // seconds or other non-negative decimal, '.' separator
    dialogueId, // node of the room's .dlg; cross-checked by dialogueReferencesError
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

// Actions run inside ScriptRunner: start once, then update every tick while running.
enum class ActionStatus : std::uint8_t { done, running };

// Per-step scratch owned by the runner, so actions stay stateless functions.
struct StepState {
    float elapsed = 0.0F; // seconds since the step started
    float value = 0.0F;   // parsed argument cached by start (e.g. wait duration)
};

struct ActionContext {
    ScriptHost& host;
    StepState& step;
};

struct EffectDescriptor {
    std::string_view keyword;
    std::string_view description;
    ArgSignature args;
    // Optional extra check after the per-ArgType one (args.size() already matches). Empty = ok.
    std::string (*validate)(ActionArgs args, const Room* room);
    ActionStatus (*start)(ActionArgs args, ActionContext& context);
    // nullptr for instant actions; otherwise polled each tick until it returns done.
    ActionStatus (*update)(ActionArgs args, ActionContext& context);
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
[[nodiscard]] bool parseNumberArgument(std::string_view text, float& value);

// "Use set_solid <objeto> <0|1>", generated from the signature.
[[nodiscard]] std::string actionUsage(std::string_view keyword, const ArgSignature& signature);

// Shared by the file parser (with line numbers) and rule validation. Empty means valid.
[[nodiscard]] std::string argumentsError(std::string_view keyword, const ArgSignature& signature,
                                         ActionArgs args, const Room* room);
[[nodiscard]] std::string effectArgumentsError(const EffectDescriptor& effect, ActionArgs args,
                                               const Room* room);
