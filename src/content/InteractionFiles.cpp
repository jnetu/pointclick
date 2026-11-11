#include "content/InteractionFiles.hpp"

#include "content/TextFormat.hpp"

#include "game/Identifier.hpp"
#include "game/Room.hpp"

#include <algorithm>
#include <locale>
#include <ostream>
#include <sstream>
#include <vector>

namespace {
using namespace TextFormat;

bool finished(std::istringstream& input) {
    input >> std::ws;
    return input.eof();
}
} // namespace

std::filesystem::path InteractionFiles::companionPath(const std::filesystem::path& roomPath) {
    auto copy = roomPath;
    copy.replace_extension(".rules");
    return copy;
}

InteractionLoadResult InteractionFiles::parse(const std::string_view content, const Room* room) {
    RoomInteractions result;
    InteractionRule* currentRule = nullptr;
    std::size_t currentHeaderLine = 0;
    std::size_t lineNumber = 0;

    const auto failAt = [](const std::size_t line, const std::string& reason) -> InteractionLoadResult {
        return {{}, "Linha " + std::to_string(line) + ": " + reason};
    };

    std::istringstream stream{std::string(content)};
    for (std::string rawLine; std::getline(stream, rawLine);) {
        ++lineNumber;
        if (rawLine.find('\0') != std::string::npos) {
            return failAt(lineNumber, "Caractere nulo invalido");
        }
        const std::string line = stripCommentPreservingQuotes(rawLine);
        if (line.empty()) continue;

        if (line.front() == '[') {
            if (line.back() != ']') {
                return failAt(lineNumber, "Cabecalho de gatilho malformado");
            }
            if (currentRule && currentRule->effects.empty()) {
                return failAt(currentHeaderLine, "Regra sem efeitos");
            }

            const std::string inside = trim(std::string_view(line).substr(1, line.size() - 2));
            std::istringstream header(inside);
            header.imbue(std::locale::classic());
            std::string triggerVerb;
            header >> triggerVerb;
            InteractionTrigger trigger;
            if (triggerVerb == "interact") {
                if (!(header >> trigger.objectId) || !finished(header)) {
                    return failAt(lineNumber, "Use [interact <objeto>]");
                }
                trigger.kind = InteractionTriggerKind::interact;
            } else if (triggerVerb == "use_item") {
                if (!(header >> trigger.itemId >> trigger.objectId) || !finished(header)) {
                    return failAt(lineNumber, "Use [use_item <item> <objeto>]");
                }
                trigger.kind = InteractionTriggerKind::useItem;
                if (trigger.itemId != InteractionTrigger::wildcard
                    && !validIdentifier(trigger.itemId)) {
                    return failAt(lineNumber, "Identificador de item invalido: " + trigger.itemId);
                }
            } else {
                return failAt(lineNumber, "Gatilho desconhecido: " + triggerVerb);
            }

            const bool anyObject = trigger.objectId == InteractionTrigger::wildcard;
            if (!anyObject && !validIdentifier(trigger.objectId)) {
                return failAt(lineNumber, "Identificador de objeto invalido: " + trigger.objectId);
            }
            if (room && !anyObject && !room->findObject(trigger.objectId)) {
                return failAt(lineNumber, "Objeto desconhecido na sala: " + trigger.objectId);
            }

            result.rules.push_back({std::move(trigger), {}, {}});
            currentRule = &result.rules.back();
            currentHeaderLine = lineNumber;
            continue;
        }

        if (!currentRule) {
            return failAt(lineNumber,
                          "Regra deve comecar com [interact <objeto>] ou [use_item <item> <objeto>]");
        }

        std::string statement = line;
        bool forcedCondition = false;
        bool forcedEffect = false;
        if (const auto equal = line.find('='); equal != std::string::npos) {
            const std::string lhs = trim(std::string_view(line).substr(0, equal));
            if (lhs == "condition") {
                forcedCondition = true;
                statement = trim(std::string_view(line).substr(equal + 1));
            } else if (lhs == "effect") {
                forcedEffect = true;
                statement = trim(std::string_view(line).substr(equal + 1));
            }
        } else if (line.starts_with("when ") || line.starts_with("when\t")) {
            forcedCondition = true;
            statement = trim(std::string_view(line).substr(5));
        }

        const auto verbEnd = std::find_if(statement.begin(), statement.end(), isSpace);
        const std::string verb(statement.begin(), verbEnd);
        if (verb.empty()) {
            return failAt(lineNumber, "Instrucao vazia na regra");
        }
        const std::string_view rest = std::string_view(statement).substr(verb.size());
        std::vector<std::string> args;

        if (!forcedEffect) {
            if (const ConditionDescriptor* condition = findCondition(verb)) {
                if (!currentRule->effects.empty()) {
                    return failAt(lineNumber, "Condicao deve vir antes dos efeitos");
                }
                std::string error = readArguments(verb, condition->args, rest, args);
                if (error.empty()) error = argumentsError(verb, condition->args, args, room);
                if (!error.empty()) return failAt(lineNumber, error);
                currentRule->conditions.push_back({condition, std::move(args)});
                continue;
            }
            if (forcedCondition) {
                return failAt(lineNumber, "Condicao desconhecida: " + verb);
            }
        }

        if (const EffectDescriptor* effect = findEffect(verb)) {
            std::string error = readArguments(verb, effect->args, rest, args);
            if (error.empty()) error = effectArgumentsError(*effect, args, room);
            if (!error.empty()) return failAt(lineNumber, error);
            currentRule->effects.push_back({effect, std::move(args)});
            continue;
        }

        return failAt(lineNumber, "Comando de regra desconhecido: " + verb);
    }

    if (stream.bad()) {
        return {{}, "Falha de leitura"};
    }
    if (currentRule && currentRule->effects.empty()) {
        return failAt(currentHeaderLine, "Regra sem efeitos");
    }
    if (const std::string validation = result.validationError(room); !validation.empty()) {
        return {{}, validation};
    }
    return {std::move(result), {}};
}

InteractionLoadResult InteractionFiles::load(const std::filesystem::path& path, const Room* room) {
    const TextLoad file = readFile(path);
    if (!file.text) return {{}, file.error};
    return parse(*file.text, room);
}

InteractionLoadResult InteractionFiles::loadOptionalForRoom(const std::filesystem::path& roomPath,
                                                            const Room* room) {
    const TextLoad file = readOptionalFile(companionPath(roomPath));
    if (!file.text) return {{}, file.error};
    if (file.text->empty()) return {RoomInteractions{}, {}};
    return parse(*file.text, room);
}

bool InteractionFiles::save(const std::filesystem::path& path,
                            const RoomInteractions& interactions,
                            std::string& error,
                            const Room* room) {
    error = interactions.validationError(room);
    if (!error.empty()) return false;

    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "# PointClick: regras declarativas de interacao\n";

    for (std::size_t i = 0; i < interactions.rules.size(); ++i) {
        const InteractionRule& rule = interactions.rules[i];
        if (i > 0) out << '\n';
        if (rule.trigger.kind == InteractionTriggerKind::interact) {
            out << "[interact " << rule.trigger.objectId << "]\n";
        } else {
            out << "[use_item " << rule.trigger.itemId << ' ' << rule.trigger.objectId << "]\n";
        }

        for (const InteractionCondition& condition : rule.conditions) {
            writeStatement(out, condition.condition->keyword, condition.condition->args,
                           condition.args);
        }
        for (const InteractionEffect& effect : rule.effects) {
            writeStatement(out, effect.effect->keyword, effect.effect->args, effect.args);
        }
    }

    return writeTextAtomic(path, out.str(), "regras", error);
}

