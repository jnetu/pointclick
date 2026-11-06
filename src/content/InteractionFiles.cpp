#include "content/InteractionFiles.hpp"

#include "game/Identifier.hpp"
#include "game/Room.hpp"

#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_stdinc.h>

#include <algorithm>
#include <fstream>
#include <locale>
#include <memory>
#include <ostream>
#include <sstream>
#include <vector>

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

bool isSpace(const char ch) {
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

void skipSpaces(const std::string_view text, std::size_t& pos) {
    while (pos < text.size() && isSpace(text[pos])) ++pos;
}

// Decodes "..." starting at pos (which must point at the opening quote).
std::string readQuotedToken(const std::string_view keyword, const std::string_view text,
                            std::size_t& pos, std::string& out) {
    std::string decoded;
    for (++pos; pos < text.size(); ++pos) {
        const char ch = text[pos];
        if (ch == '\\') {
            if (pos + 1 >= text.size()) {
                return "Sequencia de escape incompleta em " + std::string(keyword);
            }
            const char next = text[++pos];
            if (next == '"' || next == '\\') decoded.push_back(next);
            else if (next == 'n') decoded.push_back('\n');
            else return "Sequencia de escape invalida em " + std::string(keyword);
            continue;
        }
        if (ch == '"') {
            ++pos;
            if (pos < text.size() && !isSpace(text[pos])) {
                return "Caracteres extras apos aspas em " + std::string(keyword);
            }
            out = std::move(decoded);
            return {};
        }
        decoded.push_back(ch);
    }
    return "Texto entre aspas nao fechado";
}

// Tokenizes the arguments after the keyword following the catalog signature.
// Type-specific checks (identifiers, room objects) happen afterwards in argumentsError.
std::string readArguments(const std::string_view keyword, const ArgSignature& signature,
                          const std::string_view text, std::vector<std::string>& args) {
    std::size_t pos = 0;
    for (const ArgType type : signature.view()) {
        skipSpaces(text, pos);
        if (pos >= text.size()) return actionUsage(keyword, signature);
        std::string token;
        if (type == ArgType::text) {
            if (text[pos] != '"') return actionUsage(keyword, signature);
            if (std::string error = readQuotedToken(keyword, text, pos, token); !error.empty()) {
                return error;
            }
        } else {
            const std::size_t begin = pos;
            while (pos < text.size() && !isSpace(text[pos])) ++pos;
            token.assign(text.substr(begin, pos - begin));
        }
        args.push_back(std::move(token));
    }
    skipSpaces(text, pos);
    if (pos < text.size()) {
        const auto types = signature.view();
        if (!types.empty() && types.back() == ArgType::text) {
            return "Caracteres extras apos aspas em " + std::string(keyword);
        }
        return actionUsage(keyword, signature);
    }
    return {};
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

void writeStatement(std::ostream& out, const std::string_view keyword,
                    const ArgSignature& signature, const ActionArgs args) {
    out << keyword;
    const auto types = signature.view();
    for (std::size_t i = 0; i < args.size(); ++i) {
        out << ' ';
        if (types[i] == ArgType::text) out << '"' << escapeQuotedText(args[i]) << '"';
        else out << args[i];
    }
    out << '\n';
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
            if (room && !room->findObject(trigger.objectId)) {
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

    auto temporary = path;
    temporary += ".tmp";
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
            writeStatement(out, condition.condition->keyword, condition.condition->args,
                           condition.args);
        }
        for (const InteractionEffect& effect : rule.effects) {
            writeStatement(out, effect.effect->keyword, effect.effect->args, effect.args);
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
