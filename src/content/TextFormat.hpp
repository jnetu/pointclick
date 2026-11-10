#pragma once

#include "game/ActionCatalog.hpp"

#include <filesystem>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

// Line-format helpers shared by the declarative content files (.rules, .dlg): comments, quoted
// text, catalog statements and SDL-based reading. One tokenizer for every format.
namespace TextFormat {
[[nodiscard]] std::string pathToUtf8(const std::filesystem::path& path);
[[nodiscard]] std::string trim(std::string_view value);
[[nodiscard]] bool isSpace(char ch);
// Strips '#' comments only outside double-quoted strings so say "Item #1" works cleanly.
[[nodiscard]] std::string stripCommentPreservingQuotes(std::string_view line);

// Tokenizes the arguments after a catalog keyword following its signature. Type-specific checks
// (identifiers, room objects) happen afterwards in argumentsError. Empty means ok.
[[nodiscard]] std::string readArguments(std::string_view keyword, const ArgSignature& signature,
                                        std::string_view text, std::vector<std::string>& args);
// Splits "verb rest"; verb is empty for a blank statement.
void splitVerb(std::string_view statement, std::string& verb, std::string_view& rest);

// The shortest plain decimal that reads back as the same float ("300", "0.1", "53.333332"),
// independent of any locale. Every float written to a content or save file goes through here.
[[nodiscard]] std::string number(float value);

[[nodiscard]] std::string escapeQuotedText(std::string_view text);
void writeStatement(std::ostream& out, std::string_view keyword, const ArgSignature& signature,
                    ActionArgs args);

struct TextLoad {
    std::optional<std::string> text;
    std::string error;
    bool missing = false; // readOptionalFile only: there is no such file (text is empty)
};
// SDL_IOFromFile + SDL_LoadFile_IO (UTF-8 path; APK assets on Android). A leading UTF-8 BOM is
// dropped, so every format reads files saved by Windows editors.
[[nodiscard]] TextLoad readFile(const std::filesystem::path& path);
// A companion that may be absent: missing = empty text, no error. The file is found by opening
// it through SDL (assets inside an APK are not on the filesystem); the filesystem is asked only
// to tell an unreadable file from an absent one.
[[nodiscard]] TextLoad readOptionalFile(const std::filesystem::path& path);
// Writes <path>.tmp and renames it over path, so a failed write keeps the previous file.
// what names the file kind in the error ("regras", "dialogos").
[[nodiscard]] bool writeTextAtomic(const std::filesystem::path& path, std::string_view content,
                                   std::string_view what, std::string& error);
} // namespace TextFormat
