#include "content/ActorFiles.hpp"

#include "content/SpriteFields.hpp"
#include "content/TextFormat.hpp"

#include <set>
#include <sstream>

ActorLoadResult ActorFiles::parse(const std::string_view content) {
    AnimationSet animations;
    std::set<std::string, std::less<>> keys;
    std::istringstream input{std::string(content)};
    std::size_t lineNumber = 0;
    const auto fail = [&](const std::string& reason) -> ActorLoadResult {
        return {std::nullopt, "Linha " + std::to_string(lineNumber) + ": " + reason};
    };
    for (std::string raw; std::getline(input, raw);) {
        ++lineNumber;
        if (raw.find('\0') != std::string::npos) return fail("Caractere nulo invalido");
        const std::string line = TextFormat::trim(std::string_view(raw).substr(0, raw.find('#')));
        if (line.empty()) continue;
        const auto equal = line.find('=');
        if (equal == std::string::npos) return fail("Esperado chave = valor");
        const std::string key = TextFormat::trim(std::string_view(line).substr(0, equal));
        const std::string value = TextFormat::trim(std::string_view(line).substr(equal + 1));
        if (!keys.insert(key).second) return fail("Chave repetida: " + key);
        if (!SpriteFields::parseNamed(animations, key, "", value)) {
            return fail("Chave ou valor invalido: " + key);
        }
    }
    if (animations.empty()) return {std::nullopt, "Ator sem animacoes"};
    for (const auto& [name, clip] : animations) {
        if (const std::string problem = clip.validationError(); !problem.empty()) {
            return {std::nullopt, "Animacao " + name + ": " + problem};
        }
    }
    return {std::move(animations), {}};
}

namespace {
ActorLoadResult parseFile(const std::filesystem::path& path, const std::string_view text) {
    ActorLoadResult result = ActorFiles::parse(text);
    if (!result.animations) {
        result.error = TextFormat::pathToUtf8(path.filename()) + ": " + result.error;
    }
    return result;
}
} // namespace

ActorLoadResult ActorFiles::load(const std::filesystem::path& path) {
    TextFormat::TextLoad text = TextFormat::readFile(path);
    if (!text.text) return {std::nullopt, std::move(text.error)};
    return parseFile(path, *text.text);
}

ActorLoadResult ActorFiles::loadOptional(const std::filesystem::path& path) {
    TextFormat::TextLoad text = TextFormat::readOptionalFile(path);
    if (!text.text) return {std::nullopt, std::move(text.error)};
    if (text.missing) return {AnimationSet{}, {}};
    return parseFile(path, *text.text);
}
