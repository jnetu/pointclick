#include "content/InteractionFiles.hpp"

#include "game/Identifier.hpp"
#include "game/Room.hpp"

#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_stdinc.h>

#include <algorithm>
#include <fstream>
#include <locale>
#include <memory>
#include <sstream>

namespace {
std::string pathToUtf8(const std::filesystem::path& path) {
    const auto utf8 = path.u8string();
    return {reinterpret_cast<const char*>(utf8.data()), utf8.size()};
}

std::string trim(const std::string_view value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return std::string(value.substr(first, last - first + 1));
}

// Strips '#' comments only outside double-quoted strings so say "Item #1" works cleanly.
std::string stripCommentPreservingQuotes(const std::string_view line) {
    bool inQuotes = false;
    bool escaped = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (inQuotes && ch == '\\') {
            escaped = true;
            continue;
        }
        if (ch == '"') {
            inQuotes = !inQuotes;
            continue;
        }
        if (!inQuotes && ch == '#') {
            return trim(line.substr(0, i));
        }
    }
    return trim(line);
}

bool finished(std::istringstream& input) {
    input >> std::ws;
    return input.eof();
}

const SceneObject* findObjectInRoom(const Room* room, const std::string_view objectId) {
    if (!room) return nullptr;
    const auto found = std::find_if(room->scenery.begin(), room->scenery.end(),
                                    [&](const SceneObject& item) { return item.id == objectId; });
    return found == room->scenery.end() ? nullptr : &(*found);
}

struct ConditionDescriptor {
    std::string_view keyword;
    InteractionConditionKind kind;
    bool referencesObject;
};

constexpr ConditionDescriptor kConditions[]{
    {"flag_set", InteractionConditionKind::flagSet, false},
    {"flag_not_set", InteractionConditionKind::flagNotSet, false},
    {"has_item", InteractionConditionKind::hasItem, false},
    {"lacks_item", InteractionConditionKind::lacksItem, false},
    {"object_visible", InteractionConditionKind::objectVisible, true},
    {"object_hidden", InteractionConditionKind::objectHidden, true},
};

struct SingleIdEffectDescriptor {
    std::string_view keyword;
    InteractionEffectKind kind;
    bool referencesObject;
};

constexpr SingleIdEffectDescriptor kSingleIdEffects[]{
    {"set_flag", InteractionEffectKind::setFlag, false},
    {"clear_flag", InteractionEffectKind::clearFlag, false},
    {"give_item", InteractionEffectKind::giveItem, false},
    {"take_item", InteractionEffectKind::takeItem, false},
    {"hide_object", InteractionEffectKind::hideObject, true},
    {"show_object", InteractionEffectKind::showObject, true},
    {"change_room", InteractionEffectKind::changeRoom, false},
};

const ConditionDescriptor* findConditionDescriptor(const std::string_view keyword) {
    for (const auto& entry : kConditions) {
        if (entry.keyword == keyword) return &entry;
    }
    return nullptr;
}

const SingleIdEffectDescriptor* findSingleIdEffectDescriptor(const std::string_view keyword) {
    for (const auto& entry : kSingleIdEffects) {
        if (entry.keyword == keyword) return &entry;
    }
    return nullptr;
}

std::string_view conditionKeyword(const InteractionConditionKind kind) {
    for (const auto& entry : kConditions) {
        if (entry.kind == kind) return entry.keyword;
    }
    return {};
}

std::string_view singleIdEffectKeyword(const InteractionEffectKind kind) {
    for (const auto& entry : kSingleIdEffects) {
        if (entry.kind == kind) return entry.keyword;
    }
    return {};
}

bool parseQuotedText(const std::string_view rest, std::string& out, std::string& reason) {
    const std::string trimmed = trim(rest);
    if (trimmed.empty() || trimmed.front() != '"') {
        reason = "Use say \"texto\"";
        return false;
    }
    std::string decoded;
    decoded.reserve(trimmed.size());
    bool closed = false;
    for (std::size_t i = 1; i < trimmed.size(); ++i) {
        const char ch = trimmed[i];
        if (ch == '\\') {
            if (i + 1 >= trimmed.size()) {
                reason = "Sequencia de escape incompleta em say";
                return false;
            }
            const char next = trimmed[++i];
            if (next == '"' || next == '\\') decoded.push_back(next);
            else if (next == 'n') decoded.push_back('\n');
            else {
                reason = "Sequencia de escape invalida em say";
                return false;
            }
            continue;
        }
        if (ch == '"') {
            if (!trim(std::string_view(trimmed).substr(i + 1)).empty()) {
                reason = "Caracteres extras apos aspas em say";
                return false;
            }
            closed = true;
            break;
        }
        decoded.push_back(ch);
    }
    if (!closed) {
        reason = "Texto entre aspas nao fechado";
        return false;
    }
    if (decoded.empty()) {
        reason = "Texto de fala vazio";
        return false;
    }
    out = std::move(decoded);
    return true;
}

std::string escapeQuotedText(const std::string_view text) {
    std::string escaped;
    escaped.reserve(text.size() + 4);
    for (const char ch : text) {
        if (ch == '\\') escaped += "\\\\";
        else if (ch == '"') escaped += "\\\"";
        else if (ch == '\n') escaped += "\\n";
        else escaped.push_back(ch);
    }
    return escaped;
}

InteractionLoadResult loadStream(SDL_IOStream* file, const Room* room) {
    std::size_t dataSize = 0;
    std::unique_ptr<void, decltype(&SDL_free)> data(
            SDL_LoadFile_IO(file, &dataSize, false), SDL_free);
    const auto status = SDL_GetIOStatus(file);
    if (!data || (status != SDL_IO_STATUS_READY && status != SDL_IO_STATUS_EOF)) {
        return {{}, "Falha de leitura"};
    }
    return InteractionFiles::parse(
            std::string_view(static_cast<const char*>(data.get()), dataSize), room);
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
                if (!validIdentifier(trigger.itemId)) {
                    return failAt(lineNumber, "Identificador de item invalido: " + trigger.itemId);
                }
            } else {
                return failAt(lineNumber, "Gatilho desconhecido: " + triggerVerb);
            }

            if (!validIdentifier(trigger.objectId)) {
                return failAt(lineNumber, "Identificador de objeto invalido: " + trigger.objectId);
            }
            if (room && !findObjectInRoom(room, trigger.objectId)) {
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

        std::istringstream tokens(statement);
        tokens.imbue(std::locale::classic());
        std::string verb;
        tokens >> verb;
        if (verb.empty()) {
            return failAt(lineNumber, "Instrucao vazia na regra");
        }

        if (!forcedEffect) {
            if (const auto* condDesc = findConditionDescriptor(verb)) {
                if (!currentRule->effects.empty()) {
                    return failAt(lineNumber, "Condicao deve vir antes dos efeitos");
                }
                std::string targetId;
                if (!(tokens >> targetId) || !finished(tokens)) {
                    return failAt(lineNumber, "Use " + std::string(condDesc->keyword) + " <id>");
                }
                if (!validIdentifier(targetId)) {
                    return failAt(lineNumber, "Identificador invalido: " + targetId);
                }
                if (room && condDesc->referencesObject && !findObjectInRoom(room, targetId)) {
                    return failAt(lineNumber, "Objeto desconhecido na sala: " + targetId);
                }
                currentRule->conditions.push_back({condDesc->kind, std::move(targetId)});
                continue;
            }
            if (forcedCondition) {
                return failAt(lineNumber, "Condicao desconhecida: " + verb);
            }
        }

        if (verb == "say") {
            const auto verbPos = statement.find("say");
            const std::string_view rest = std::string_view(statement).substr(verbPos + 3);
            std::string spoken;
            std::string reason;
            if (!parseQuotedText(rest, spoken, reason)) {
                return failAt(lineNumber, reason);
            }
            currentRule->effects.push_back(InteractionEffect::say(std::move(spoken)));
            continue;
        }

        if (const auto* effectDesc = findSingleIdEffectDescriptor(verb)) {
            std::string targetId;
            if (!(tokens >> targetId) || !finished(tokens)) {
                return failAt(lineNumber, "Use " + std::string(effectDesc->keyword) + " <id>");
            }
            if (!validIdentifier(targetId)) {
                return failAt(lineNumber, "Identificador invalido: " + targetId);
            }
            if (room && effectDesc->referencesObject && !findObjectInRoom(room, targetId)) {
                return failAt(lineNumber, "Objeto desconhecido na sala: " + targetId);
            }
            currentRule->effects.push_back({effectDesc->kind, std::move(targetId), {}, false});
            continue;
        }

        if (verb == "set_solid") {
            std::string objectId;
            std::string flagToken;
            if (!(tokens >> objectId >> flagToken) || !finished(tokens)
                || (flagToken != "0" && flagToken != "1")) {
                return failAt(lineNumber, "Use set_solid <objeto> <0|1>");
            }
            if (!validIdentifier(objectId)) {
                return failAt(lineNumber, "Identificador invalido: " + objectId);
            }
            if (room && !findObjectInRoom(room, objectId)) {
                return failAt(lineNumber, "Objeto desconhecido na sala: " + objectId);
            }
            currentRule->effects.push_back(
                    InteractionEffect::setSolid(std::move(objectId), flagToken == "1"));
            continue;
        }

        if (verb == "play_animation") {
            std::string objectId;
            std::string animName;
            if (!(tokens >> objectId >> animName) || !finished(tokens)) {
                return failAt(lineNumber, "Use play_animation <objeto> <animacao>");
            }
            if (!validIdentifier(objectId) || !validIdentifier(animName)) {
                return failAt(lineNumber, "Identificador invalido em play_animation");
            }
            if (room) {
                const SceneObject* target = findObjectInRoom(room, objectId);
                if (!target) {
                    return failAt(lineNumber, "Objeto desconhecido na sala: " + objectId);
                }
                if (!findAnimation(target->animations, animName)) {
                    return failAt(lineNumber,
                                  "Animacao desconhecida '" + animName + "' no objeto: " + objectId);
                }
            }
            currentRule->effects.push_back(
                    InteractionEffect::playAnimation(std::move(objectId), std::move(animName)));
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
    const auto filename = pathToUtf8(path);
    std::unique_ptr<SDL_IOStream, decltype(&SDL_CloseIO)> file(
            SDL_IOFromFile(filename.c_str(), "rb"), SDL_CloseIO);
    if (!file) {
        return {{}, "Nao foi possivel abrir " + pathToUtf8(path.filename())};
    }
    return loadStream(file.get(), room);
}

InteractionLoadResult InteractionFiles::loadOptionalForRoom(const std::filesystem::path& roomPath,
                                                            const Room* room) {
    const auto rulesPath = companionPath(roomPath);
    const auto filename = pathToUtf8(rulesPath);
    std::unique_ptr<SDL_IOStream, decltype(&SDL_CloseIO)> file(
            SDL_IOFromFile(filename.c_str(), "rb"), SDL_CloseIO);
    if (!file) {
        std::error_code ec;
        if (std::filesystem::exists(rulesPath, ec)) {
            return {{}, "Nao foi possivel abrir " + pathToUtf8(rulesPath.filename())};
        }
        return {RoomInteractions{}, {}};
    }
    return loadStream(file.get(), room);
}

bool InteractionFiles::save(const std::filesystem::path& path,
                            const RoomInteractions& interactions,
                            std::string& error,
                            const Room* room) {
    error = interactions.validationError(room);
    if (!error.empty()) return false;

    const auto temporary = std::filesystem::path(path.string() + ".tmp");
    std::ofstream out(temporary);
    if (!out) {
        error = "Nao foi possivel salvar " + pathToUtf8(path.filename());
        return false;
    }
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
            out << conditionKeyword(condition.kind) << ' ' << condition.targetId << '\n';
        }

        for (const InteractionEffect& effect : rule.effects) {
            switch (effect.kind) {
            case InteractionEffectKind::say:
                out << "say \"" << escapeQuotedText(effect.value) << "\"\n";
                break;
            case InteractionEffectKind::setSolid:
                out << "set_solid " << effect.targetId << ' ' << (effect.boolValue ? 1 : 0) << '\n';
                break;
            case InteractionEffectKind::playAnimation:
                out << "play_animation " << effect.targetId << ' ' << effect.value << '\n';
                break;
            default:
                out << singleIdEffectKeyword(effect.kind) << ' ' << effect.targetId << '\n';
                break;
            }
        }
    }

    out.close();
    std::error_code ioError;
    if (out) std::filesystem::rename(temporary, path, ioError);
    if (!out || ioError) {
        std::filesystem::remove(temporary, ioError);
        error = "Falha ao gravar o arquivo de regras";
        return false;
    }
    return true;
}
