#pragma once

#include "game/Interaction.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

struct Room;
class ScriptHost;
class ScriptRunner;
class SessionState;

// ---- Authored definitions (from <room>.dlg). Immutable during gameplay. ----

// One option of a node. Conditions (the .rules condition catalog) decide whether it is listed
// when the node opens; effects (the effect catalog) run as one sequence when it is chosen.
struct DialogueChoice {
    std::string id;   // stable within the node; keys the once-progress in SessionState
    std::string text; // what the menu shows
    std::vector<InteractionCondition> conditions;
    std::vector<InteractionEffect> effects;
    bool once = false; // hidden after being chosen once (SessionState remembers it)
    std::string next;  // node opened after the effects; empty ends the conversation

    [[nodiscard]] std::string validationError(const Room* room) const;
};

struct DialogueNode {
    std::string id;
    std::vector<InteractionEffect> lines; // run when the node opens, before the menu shows
    std::vector<DialogueChoice> choices;

    [[nodiscard]] const DialogueChoice* findChoice(std::string_view choiceId) const;
    [[nodiscard]] std::string validationError(const Room* room) const;
};

struct RoomDialogues {
    std::vector<DialogueNode> nodes;

    [[nodiscard]] bool empty() const noexcept { return nodes.empty(); }
    [[nodiscard]] const DialogueNode* findNode(std::string_view nodeId) const;
    // Unique IDs, known goto targets, valid steps (start_dialogue is not allowed inside a node:
    // use the choice's goto). Empty means valid.
    [[nodiscard]] std::string validationError(const Room* room = nullptr) const;
};

// Every start_dialogue in the room's rules must name a node of its dialogues.
[[nodiscard]] std::string dialogueReferencesError(const RoomInteractions& interactions,
                                                  const RoomDialogues& dialogues);

// ---- Runtime (scene simulation): the conversation in progress. ----

enum class DialoguePhase : std::uint8_t {
    idle,
    lines,    // the node's opening steps hold the runner
    choosing, // the menu waits for chooseOption
    reply,    // the chosen option's effects hold the runner
};

// Drives one conversation over the shared ScriptRunner: a node's lines and a choice's effects are
// pushed as sequences on top of whatever opened the dialogue. Holds pointers into the
// RoomDialogues it was given, so the owner cancels it before replacing them.
class DialoguePlayer {
public:
    // Deferred open, so a start_dialogue action never re-enters the runner. Unknown node = false.
    bool request(const RoomDialogues& dialogues, std::string_view nodeId);
    // Opens requested nodes and moves the phase once the runner is back below the sequence this
    // dialogue pushed. Call after every runner call.
    void settle(ScriptRunner& runner, ScriptHost& host);
    // Index into options(). Records once-progress and starts the reply. False when not choosing.
    bool choose(std::size_t index, ScriptRunner& runner, ScriptHost& host);
    void cancel() noexcept;

    [[nodiscard]] bool active() const noexcept { return phase_ != DialoguePhase::idle || pending_; }
    [[nodiscard]] DialoguePhase phase() const noexcept { return phase_; }
    [[nodiscard]] const DialogueNode* node() const noexcept { return node_; }
    // Options listed when the node's lines ended, in authored order; valid while choosing.
    [[nodiscard]] std::span<const DialogueChoice* const> options() const noexcept {
        return options_;
    }

private:
    void open(const DialogueNode& node, ScriptRunner& runner, ScriptHost& host);
    void finish() noexcept;

    const RoomDialogues* dialogues_ = nullptr;
    const DialogueNode* node_ = nullptr;
    const DialogueNode* pending_ = nullptr;
    const DialogueNode* afterReply_ = nullptr;
    std::vector<const DialogueChoice*> options_;
    std::size_t sequenceDepth_ = 0; // runner depth just below the sequence this dialogue pushed
    DialoguePhase phase_ = DialoguePhase::idle;
};
