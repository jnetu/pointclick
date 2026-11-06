#include "content/DialogueFiles.hpp"

#include "content/TextFormat.hpp"
#include "game/Identifier.hpp"

#include <locale>
#include <sstream>
#include <utility>
#include <vector>

namespace {
using namespace TextFormat;

constexpr ArgSignature kNodeHeader{ArgType::dialogueId};
constexpr ArgSignature kChoiceHeader{ArgType::dialogueId, ArgType::text};
constexpr ArgSignature kGoto{ArgType::dialogueId};

std::string readEffect(const std::string& verb, const std::string_view rest, const Room* room,
                       std::vector<InteractionEffect>& target) {
    const EffectDescriptor* effect = findEffect(verb);
    if (!effect) return "Comando de dialogo desconhecido: " + verb;
    if (effect->keyword == "start_dialogue") {
        return "start_dialogue nao vale dentro de dialogos; use goto <no>";
    }
    std::vector<std::string> args;
    std::string error = readArguments(verb, effect->args, rest, args);
    if (error.empty()) error = effectArgumentsError(*effect, args, room);
    if (error.empty()) target.push_back({effect, std::move(args)});
    return error;
}

struct PendingGoto {
    std::size_t line;
    std::string target;
};
} // namespace

std::filesystem::path DialogueFiles::companionPath(const std::filesystem::path& roomPath) {
    auto copy = roomPath;
    copy.replace_extension(".dlg");
    return copy;
}

DialogueLoadResult DialogueFiles::parse(const std::string_view content, const Room* room) {
    RoomDialogues result;
    DialogueNode* node = nullptr;
    DialogueChoice* choice = nullptr;
    bool choiceEnded = false; // goto seen: it must be the choice's last line
    std::size_t nodeLine = 0;
    std::vector<PendingGoto> gotos;
    std::size_t lineNumber = 0;

    const auto failAt = [](const std::size_t line, const std::string& reason) -> DialogueLoadResult {
        return {{}, "Linha " + std::to_string(line) + ": " + reason};
    };
    const auto nodeEmpty = [&] { return node && node->lines.empty() && node->choices.empty(); };

    std::istringstream stream{std::string(content)};
    for (std::string rawLine; std::getline(stream, rawLine);) {
        ++lineNumber;
        if (rawLine.find('\0') != std::string::npos) {
            return failAt(lineNumber, "Caractere nulo invalido");
        }
        const std::string line = stripCommentPreservingQuotes(rawLine);
        if (line.empty()) continue;

        if (line.front() == '[') {
            if (line.back() != ']') return failAt(lineNumber, "Cabecalho de dialogo malformado");
            const std::string inside = trim(std::string_view(line).substr(1, line.size() - 2));
            std::string verb;
            std::string_view rest;
            splitVerb(inside, verb, rest);
            std::vector<std::string> args;
            if (verb == "node") {
                if (nodeEmpty()) return failAt(nodeLine, "No de dialogo vazio: " + node->id);
                if (!readArguments(verb, kNodeHeader, rest, args).empty()) {
                    return failAt(lineNumber, "Use [node <id>]");
                }
                if (!validIdentifier(args[0])) {
                    return failAt(lineNumber, "Identificador de no invalido: " + args[0]);
                }
                if (result.findNode(args[0])) {
                    return failAt(lineNumber, "No de dialogo repetido: " + args[0]);
                }
                result.nodes.push_back({std::move(args[0]), {}, {}});
                node = &result.nodes.back();
                choice = nullptr;
                nodeLine = lineNumber;
                continue;
            }
            if (verb == "choice") {
                if (!node) return failAt(lineNumber, "Escolha deve vir depois de um [node <id>]");
                if (!readArguments(verb, kChoiceHeader, rest, args).empty()) {
                    return failAt(lineNumber, "Use [choice <id> \"texto\"]");
                }
                if (!validIdentifier(args[0])) {
                    return failAt(lineNumber, "Identificador de escolha invalido: " + args[0]);
                }
                if (args[1].empty()) return failAt(lineNumber, "Escolha sem texto: " + args[0]);
                if (node->findChoice(args[0])) {
                    return failAt(lineNumber, "Escolha repetida no no " + node->id + ": " + args[0]);
                }
                DialogueChoice next;
                next.id = std::move(args[0]);
                next.text = std::move(args[1]);
                node->choices.push_back(std::move(next));
                choice = &node->choices.back();
                choiceEnded = false;
                continue;
            }
            return failAt(lineNumber, "Bloco desconhecido: " + verb
                                      + " (use [node <id>] ou [choice <id> \"texto\"])");
        }

        if (!node) return failAt(lineNumber, "Dialogo deve comecar com [node <id>]");
        std::string verb;
        std::string_view rest;
        splitVerb(line, verb, rest);

        if (!choice) {
            if (verb == "goto" || verb == "once" || findCondition(verb)) {
                return failAt(lineNumber, verb + " so vale dentro de [choice <id> \"texto\"]");
            }
            if (std::string error = readEffect(verb, rest, room, node->lines); !error.empty()) {
                return failAt(lineNumber, error);
            }
            continue;
        }

        if (choiceEnded) return failAt(lineNumber, "goto deve ser a ultima linha da escolha");
        if (verb == "goto") {
            std::vector<std::string> args;
            if (!readArguments(verb, kGoto, rest, args).empty() || !validIdentifier(args[0])) {
                return failAt(lineNumber, "Use goto <no>");
            }
            choice->next = args[0];
            gotos.push_back({lineNumber, std::move(args[0])});
            choiceEnded = true;
            continue;
        }
        if (verb == "once") {
            if (!trim(rest).empty()) return failAt(lineNumber, "Use once");
            if (choice->once) return failAt(lineNumber, "once repetido na escolha");
            choice->once = true;
            continue;
        }
        if (const ConditionDescriptor* condition = findCondition(verb)) {
            if (!choice->effects.empty()) {
                return failAt(lineNumber, "Condicao deve vir antes dos efeitos");
            }
            std::vector<std::string> args;
            std::string error = readArguments(verb, condition->args, rest, args);
            if (error.empty()) error = argumentsError(verb, condition->args, args, room);
            if (!error.empty()) return failAt(lineNumber, error);
            choice->conditions.push_back({condition, std::move(args)});
            continue;
        }
        if (std::string error = readEffect(verb, rest, room, choice->effects); !error.empty()) {
            return failAt(lineNumber, error);
        }
    }

    if (stream.bad()) return {{}, "Falha de leitura"};
    if (nodeEmpty()) return failAt(nodeLine, "No de dialogo vazio: " + node->id);
    for (const PendingGoto& pending : gotos) {
        if (!result.findNode(pending.target)) {
            return failAt(pending.line, "Destino de goto desconhecido: " + pending.target);
        }
    }
    if (std::string validation = result.validationError(room); !validation.empty()) {
        return {{}, std::move(validation)};
    }
    return {std::move(result), {}};
}

DialogueLoadResult DialogueFiles::load(const std::filesystem::path& path, const Room* room) {
    const TextLoad file = readFile(path);
    if (!file.text) return {{}, file.error};
    return parse(*file.text, room);
}

DialogueLoadResult DialogueFiles::loadOptionalForRoom(const std::filesystem::path& roomPath,
                                                      const Room* room) {
    const TextLoad file = readOptionalFile(companionPath(roomPath));
    if (!file.text) return {{}, file.error};
    if (file.text->empty()) return {RoomDialogues{}, {}};
    return parse(*file.text, room);
}

bool DialogueFiles::save(const std::filesystem::path& path, const RoomDialogues& dialogues,
                         std::string& error, const Room* room) {
    error = dialogues.validationError(room);
    if (!error.empty()) return false;

    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "# PointClick: dialogos da sala\n";
    for (const DialogueNode& node : dialogues.nodes) {
        out << "\n[node " << node.id << "]\n";
        for (const InteractionEffect& line : node.lines) {
            writeStatement(out, line.effect->keyword, line.effect->args, line.args);
        }
        for (const DialogueChoice& choice : node.choices) {
            out << "\n[choice " << choice.id << " \"" << escapeQuotedText(choice.text) << "\"]\n";
            for (const InteractionCondition& condition : choice.conditions) {
                writeStatement(out, condition.condition->keyword, condition.condition->args,
                               condition.args);
            }
            if (choice.once) out << "once\n";
            for (const InteractionEffect& effect : choice.effects) {
                writeStatement(out, effect.effect->keyword, effect.effect->args, effect.args);
            }
            if (!choice.next.empty()) out << "goto " << choice.next << '\n';
        }
    }
    return writeTextAtomic(path, out.str(), "dialogos", error);
}
