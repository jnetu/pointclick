#include "content/TextFormat.hpp"

#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_stdinc.h>

#include <algorithm>
#include <fstream>
#include <memory>
#include <system_error>

namespace {
void skipSpaces(const std::string_view text, std::size_t& pos) {
    while (pos < text.size() && TextFormat::isSpace(text[pos])) ++pos;
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
            if (pos < text.size() && !TextFormat::isSpace(text[pos])) {
                return "Caracteres extras apos aspas em " + std::string(keyword);
            }
            out = std::move(decoded);
            return {};
        }
        decoded.push_back(ch);
    }
    return "Texto entre aspas nao fechado";
}

using FileHandle = std::unique_ptr<SDL_IOStream, decltype(&SDL_CloseIO)>;

FileHandle openForReading(const std::filesystem::path& path) {
    const std::string filename = TextFormat::pathToUtf8(path);
    return {SDL_IOFromFile(filename.c_str(), "rb"), SDL_CloseIO};
}

TextFormat::TextLoad readStream(SDL_IOStream* file) {
    std::size_t dataSize = 0;
    std::unique_ptr<void, decltype(&SDL_free)> data(
            SDL_LoadFile_IO(file, &dataSize, false), SDL_free);
    const auto status = SDL_GetIOStatus(file);
    if (!data || (status != SDL_IO_STATUS_READY && status != SDL_IO_STATUS_EOF)) {
        return {std::nullopt, "Falha de leitura"};
    }
    return {std::string(static_cast<const char*>(data.get()), dataSize), {}};
}
} // namespace

std::string TextFormat::pathToUtf8(const std::filesystem::path& path) {
    const auto utf8 = path.u8string();
    return {reinterpret_cast<const char*>(utf8.data()), utf8.size()};
}

std::string TextFormat::trim(const std::string_view value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return std::string(value.substr(first, last - first + 1));
}

bool TextFormat::isSpace(const char ch) {
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

std::string TextFormat::stripCommentPreservingQuotes(const std::string_view line) {
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

std::string TextFormat::readArguments(const std::string_view keyword,
                                      const ArgSignature& signature,
                                      const std::string_view text,
                                      std::vector<std::string>& args) {
    std::size_t pos = 0;
    const auto signatureTypes = signature.view();
    for (std::size_t index = 0; index < signatureTypes.size(); ++index) {
        const ArgType type = signatureTypes[index];
        skipSpaces(text, pos);
        if (pos >= text.size()) {
            if (index >= signature.required) return {}; // optional tail left out
            return actionUsage(keyword, signature);
        }
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

void TextFormat::splitVerb(const std::string_view statement, std::string& verb,
                           std::string_view& rest) {
    const auto verbEnd = std::find_if(statement.begin(), statement.end(), isSpace);
    verb.assign(statement.begin(), verbEnd);
    rest = statement.substr(verb.size());
}

std::string TextFormat::escapeQuotedText(const std::string_view text) {
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

void TextFormat::writeStatement(std::ostream& out, const std::string_view keyword,
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

TextFormat::TextLoad TextFormat::readFile(const std::filesystem::path& path) {
    const FileHandle file = openForReading(path);
    if (!file) {
        return {std::nullopt, "Nao foi possivel abrir " + pathToUtf8(path.filename())};
    }
    return readStream(file.get());
}

TextFormat::TextLoad TextFormat::readOptionalFile(const std::filesystem::path& path) {
    const FileHandle file = openForReading(path);
    if (!file) {
        std::error_code ec;
        if (std::filesystem::exists(path, ec)) {
            return {std::nullopt, "Nao foi possivel abrir " + pathToUtf8(path.filename())};
        }
        return {std::string(), {}};
    }
    return readStream(file.get());
}

bool TextFormat::writeTextAtomic(const std::filesystem::path& path, const std::string_view content,
                                 const std::string_view what, std::string& error) {
    auto temporary = path;
    temporary += ".tmp";
    std::ofstream out(temporary, std::ios::binary);
    if (!out) {
        error = "Nao foi possivel salvar " + pathToUtf8(path.filename());
        return false;
    }
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    out.close();
    std::error_code ioError;
    if (out) std::filesystem::rename(temporary, path, ioError);
    if (!out || ioError) {
        std::filesystem::remove(temporary, ioError);
        error = "Falha ao gravar o arquivo de " + std::string(what);
        return false;
    }
    return true;
}
