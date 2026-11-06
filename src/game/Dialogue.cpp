#include "game/Dialogue.hpp"

#include "game/Identifier.hpp"
#include "game/ScriptHost.hpp"
#include "game/ScriptRunner.hpp"
#include "game/SessionState.hpp"

#include <algorithm>

namespace {
// Flow inside a conversation is the choice's goto; nesting start_dialogue would stack
// conversations the menu cannot show.
std::string dialogueStepError(const InteractionEffect& effect, const Room* room) {
    if (std::string error = effect.validationError(room); !error.empty()) return error;
    if (effect.effect->keyword == "start_dialogue") {
        return "start_dialogue nao vale dentro de dialogos; use goto <no>";
    }
    return {};
}

bool choiceAvailable(const DialogueChoice& choice, const DialogueNode& node,
                     const SessionState& session, const std::string_view roomId) {
    if (choice.once && session.dialogueChoiceTaken(roomId, node.id, choice.id)) return false;
    return std::all_of(choice.conditions.begin(), choice.conditions.end(),
                       [&](const InteractionCondition& condition) {
                           return condition.evaluate(session, roomId);
                       });
}
} // namespace

std::string DialogueChoice::validationError(const Room* room) const {
    if (!validIdentifier(id)) return "Identificador de escolha invalido: " + id;
    if (text.empty()) return "Escolha sem texto: " + id;
    for (const InteractionCondition& condition : conditions) {
        if (std::string error = condition.validationError(room); !error.empty()) return error;
    }
    for (const InteractionEffect& effect : effects) {
        if (std::string error = dialogueStepError(effect, room); !error.empty()) return error;
    }
    if (!next.empty() && !validIdentifier(next)) return "Identificador invalido em goto: " + next;
    return {};
}

const DialogueChoice* DialogueNode::findChoice(const std::string_view choiceId) const {
    const auto found = std::find_if(choices.begin(), choices.end(),
                                    [&](const DialogueChoice& choice) { return choice.id == choiceId; });
    return found == choices.end() ? nullptr : &*found;
}

std::string DialogueNode::validationError(const Room* room) const {
    if (!validIdentifier(id)) return "Identificador de no invalido: " + id;
    if (lines.empty() && choices.empty()) return "No de dialogo vazio: " + id;
    for (const InteractionEffect& line : lines) {
        if (std::string error = dialogueStepError(line, room); !error.empty()) return error;
    }
    for (std::size_t i = 0; i < choices.size(); ++i) {
        if (std::string error = choices[i].validationError(room); !error.empty()) return error;
        for (std::size_t j = 0; j < i; ++j) {
            if (choices[j].id == choices[i].id) {
                return "Escolha repetida no no " + id + ": " + choices[i].id;
            }
        }
    }
    return {};
}

const DialogueNode* RoomDialogues::findNode(const std::string_view nodeId) const {
    const auto found = std::find_if(nodes.begin(), nodes.end(),
                                    [&](const DialogueNode& node) { return node.id == nodeId; });
    return found == nodes.end() ? nullptr : &*found;
}

std::string RoomDialogues::validationError(const Room* room) const {
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        if (std::string error = nodes[i].validationError(room); !error.empty()) return error;
        for (std::size_t j = 0; j < i; ++j) {
            if (nodes[j].id == nodes[i].id) return "No de dialogo repetido: " + nodes[i].id;
        }
    }
    for (const DialogueNode& node : nodes) {
        for (const DialogueChoice& choice : node.choices) {
            if (!choice.next.empty() && !findNode(choice.next)) {
                return "Destino de goto desconhecido: " + choice.next;
            }
        }
    }
    return {};
}

std::string dialogueReferencesError(const RoomInteractions& interactions,
                                    const RoomDialogues& dialogues) {
    for (const InteractionRule& rule : interactions.rules) {
        for (const InteractionEffect& effect : rule.effects) {
            if (!effect.effect) continue;
            const auto types = effect.effect->args.view();
            for (std::size_t i = 0; i < types.size() && i < effect.args.size(); ++i) {
                if (types[i] == ArgType::dialogueId && !dialogues.findNode(effect.args[i])) {
                    return "Dialogo desconhecido: " + effect.args[i];
                }
            }
        }
    }
    return {};
}

bool DialoguePlayer::request(const RoomDialogues& dialogues, const std::string_view nodeId) {
    const DialogueNode* node = dialogues.findNode(nodeId);
    if (!node) return false;
    dialogues_ = &dialogues;
    pending_ = node;
    return true;
}

void DialoguePlayer::open(const DialogueNode& node, ScriptRunner& runner, ScriptHost& host) {
    node_ = &node;
    options_.clear();
    phase_ = DialoguePhase::lines;
    sequenceDepth_ = runner.depth();
    runner.push(node.lines, host);
}

void DialoguePlayer::settle(ScriptRunner& runner, ScriptHost& host) {
    // Each pass either opens a node or moves one phase; a goto chain of nodes without lines or
    // options could cycle, so the passes are bounded and the rest waits for the next settle.
    for (int pass = 0; pass < 16; ++pass) {
        if (pending_) {
            const DialogueNode* next = pending_;
            pending_ = nullptr;
            open(*next, runner, host);
            continue;
        }
        if (runner.depth() > sequenceDepth_) return; // lines or reply still running
        if (phase_ == DialoguePhase::lines) {
            options_.clear();
            for (const DialogueChoice& choice : node_->choices) {
                if (choiceAvailable(choice, *node_, host.session(), host.roomId())) {
                    options_.push_back(&choice);
                }
            }
            if (options_.empty()) {
                finish();
            } else {
                phase_ = DialoguePhase::choosing;
            }
            return;
        }
        if (phase_ == DialoguePhase::reply) {
            if (afterReply_) {
                pending_ = afterReply_;
                afterReply_ = nullptr;
                continue;
            }
            finish();
        }
        return;
    }
}

bool DialoguePlayer::choose(const std::size_t index, ScriptRunner& runner, ScriptHost& host) {
    if (phase_ != DialoguePhase::choosing || index >= options_.size()) return false;
    const DialogueChoice& choice = *options_[index];
    if (choice.once) host.session().markDialogueChoice(host.roomId(), node_->id, choice.id);
    afterReply_ = choice.next.empty() ? nullptr : dialogues_->findNode(choice.next);
    options_.clear();
    phase_ = DialoguePhase::reply;
    sequenceDepth_ = runner.depth();
    // A reply that leads to another node is read to the end before that node opens.
    runner.push(choice.effects, host, afterReply_ != nullptr);
    settle(runner, host);
    return true;
}

void DialoguePlayer::cancel() noexcept {
    finish();
}

void DialoguePlayer::finish() noexcept {
    dialogues_ = nullptr;
    node_ = nullptr;
    pending_ = nullptr;
    afterReply_ = nullptr;
    options_.clear();
    sequenceDepth_ = 0;
    phase_ = DialoguePhase::idle;
}
