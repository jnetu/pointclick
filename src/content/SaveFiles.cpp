#include "content/SaveFiles.hpp"

#include "content/TextFormat.hpp"
#include "game/Identifier.hpp"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <locale>
#include <map>
#include <set>
#include <sstream>
#include <system_error>
#include <tuple>
#include <utility>

namespace {
using TextFormat::trim;

bool parseInteger(const std::string_view text, int& value) {
    const char* end = text.data() + text.size();
    const auto [last, status] = std::from_chars(text.data(), end, value);
    return status == std::errc() && last == end;
}

bool parseBoolean(const std::string_view text, bool& value) {
    if (text != "0" && text != "1") return false;
    value = text == "1";
    return true;
}

bool parsePoint(std::string text, SDL_FPoint& point) {
    std::replace(text.begin(), text.end(), ',', ' ');
    std::istringstream input(text);
    input.imbue(std::locale::classic());
    if (!(input >> point.x >> point.y) || !std::isfinite(point.x) || !std::isfinite(point.y)) {
        return false;
    }
    input >> std::ws;
    return input.eof();
}

// "a/b/c" with three identifiers.
bool parseDialogueChoice(const std::string_view text, SessionSnapshot::DialogueChoice& choice) {
    const auto first = text.find('/');
    if (first == std::string_view::npos) return false;
    const auto second = text.find('/', first + 1);
    if (second == std::string_view::npos) return false;
    choice = {std::string(text.substr(0, first)),
              std::string(text.substr(first + 1, second - first - 1)),
              std::string(text.substr(second + 1))};
    return validIdentifier(choice.roomId) && validIdentifier(choice.nodeId)
           && validIdentifier(choice.choiceId);
}

// "object.<room>.<object>.<field>"; identifiers never contain '.'.
bool splitObjectKey(const std::string_view key, std::string& roomId, std::string& objectId,
                    std::string_view& field) {
    const std::string_view rest = key.substr(std::string_view("object.").size());
    const auto first = rest.find('.');
    if (first == std::string_view::npos) return false;
    const auto second = rest.find('.', first + 1);
    if (second == std::string_view::npos) return false;
    roomId = rest.substr(0, first);
    objectId = rest.substr(first + 1, second - first - 1);
    field = rest.substr(second + 1);
    return validIdentifier(roomId) && validIdentifier(objectId);
}
} // namespace

SaveLoadResult SaveFiles::parse(const std::string_view text) {
    std::istringstream input{std::string(text)};
    SavedGame save;
    std::optional<int> version;
    bool hasRoom = false;
    bool hasFeet = false;
    std::set<std::string> keys;
    std::set<std::string, std::less<>> items;
    std::map<std::pair<std::string, std::string>, SessionSnapshot::ObjectOverride> objects;
    std::size_t lineNumber = 0;
    const auto fail = [&](const std::string& reason) -> SaveLoadResult {
        return {std::nullopt, "Linha " + std::to_string(lineNumber) + ": " + reason};
    };
    for (std::string line; std::getline(input, line);) {
        ++lineNumber;
        line = trim(line.substr(0, line.find('#')));
        if (line.empty()) continue;
        const auto equal = line.find('=');
        if (equal == std::string::npos) return fail("Esperado chave = valor");
        const std::string key = trim(line.substr(0, equal));
        const std::string value = trim(line.substr(equal + 1));
        const bool repeatable = key == "item" || key == "dialogue";
        if (!repeatable && !keys.insert(key).second) return fail("Chave repetida: " + key);

        // The version decides how the rest is read, so it comes first.
        if (!version) {
            int number = 0;
            if (key != "version") return fail("A primeira chave deve ser version");
            if (!parseInteger(value, number)) return fail("Chave ou valor invalido: version");
            if (number < 1 || number > currentVersion) {
                return fail("Versao de save desconhecida: " + value);
            }
            version = number;
            continue;
        }
        bool parsed = false;
        if (key == "room") {
            save.roomId = value;
            parsed = hasRoom = validIdentifier(value);
        } else if (key == "feet") {
            parsed = hasFeet = parsePoint(value, save.feet);
        } else if (key == "facing" && *version >= 2) {
            const auto facing = parseFacing(value);
            parsed = facing.has_value();
            if (parsed) save.facing = *facing;
        } else if (key.starts_with("flag.")) {
            SessionSnapshot::Flag flag{key.substr(5), 0};
            parsed = validIdentifier(flag.id) && parseInteger(value, flag.value)
                     && flag.value != 0;
            if (parsed) save.session.flags.push_back(std::move(flag));
        } else if (key == "item") {
            parsed = validIdentifier(value);
            if (parsed && !items.insert(value).second) return fail("Item repetido: " + value);
            if (parsed) save.session.items.push_back(value);
        } else if (key.starts_with("object.")) {
            std::string roomId;
            std::string objectId;
            std::string_view field;
            bool flagValue = false;
            if (splitObjectKey(key, roomId, objectId, field) && parseBoolean(value, flagValue)
                && (field == "visible" || field == "solid")) {
                auto& entry = objects[{roomId, objectId}];
                entry.roomId = roomId;
                entry.objectId = objectId;
                (field == "visible" ? entry.visible : entry.solidOverride) = flagValue;
                parsed = true;
            }
        } else if (key == "dialogue") {
            SessionSnapshot::DialogueChoice choice;
            parsed = parseDialogueChoice(value, choice);
            if (parsed) {
                if (std::find(save.session.dialogueChoices.begin(),
                              save.session.dialogueChoices.end(), choice)
                    != save.session.dialogueChoices.end()) {
                    return fail("Escolha de dialogo repetida: " + value);
                }
                save.session.dialogueChoices.push_back(std::move(choice));
            }
        } else {
            return fail("Chave desconhecida: " + key);
        }
        if (!parsed) return fail("Chave ou valor invalido: " + key);
    }
    if (input.bad()) return {std::nullopt, "Falha de leitura"};
    if (!version) return {std::nullopt, "Save vazio ou sem version"};
    if (!hasRoom) return {std::nullopt, "Falta a chave room"};
    if (!hasFeet) return {std::nullopt, "Falta a chave feet"};

    // Same canonical order as SessionState::snapshot(), so parse(serialize(x)) == x.
    std::sort(save.session.flags.begin(), save.session.flags.end(),
              [](const auto& a, const auto& b) { return a.id < b.id; });
    for (auto& [ids, entry] : objects) save.session.objects.push_back(std::move(entry));
    std::sort(save.session.dialogueChoices.begin(), save.session.dialogueChoices.end(),
              [](const auto& a, const auto& b) {
                  return std::tie(a.roomId, a.nodeId, a.choiceId)
                         < std::tie(b.roomId, b.nodeId, b.choiceId);
              });
    if (std::string error = save.session.validationError(); !error.empty()) {
        return {std::nullopt, std::move(error)};
    }
    return {std::move(save), {}};
}

std::string SaveFiles::serialize(const SavedGame& save) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "# PointClick: progresso da partida (gerado pelo jogo)\n"
        << "version = " << currentVersion << '\n'
        << "room = " << save.roomId << '\n'
        << "feet = " << TextFormat::number(save.feet.x) << ", "
        << TextFormat::number(save.feet.y) << '\n'
        << "facing = " << facingName(save.facing) << '\n';
    // Sorted like snapshot(), whatever order the caller built the lists in.
    SessionSnapshot session = save.session;
    std::sort(session.flags.begin(), session.flags.end(),
              [](const auto& a, const auto& b) { return a.id < b.id; });
    std::sort(session.objects.begin(), session.objects.end(), [](const auto& a, const auto& b) {
        return std::tie(a.roomId, a.objectId) < std::tie(b.roomId, b.objectId);
    });
    std::sort(session.dialogueChoices.begin(), session.dialogueChoices.end(),
              [](const auto& a, const auto& b) {
                  return std::tie(a.roomId, a.nodeId, a.choiceId)
                         < std::tie(b.roomId, b.nodeId, b.choiceId);
              });
    for (const auto& flag : session.flags) out << "flag." << flag.id << " = " << flag.value << '\n';
    for (const auto& item : session.items) out << "item = " << item << '\n';
    for (const auto& object : session.objects) {
        const std::string prefix = "object." + object.roomId + "." + object.objectId;
        if (object.visible) out << prefix << ".visible = " << (*object.visible ? 1 : 0) << '\n';
        if (object.solidOverride) {
            out << prefix << ".solid = " << (*object.solidOverride ? 1 : 0) << '\n';
        }
    }
    for (const auto& choice : session.dialogueChoices) {
        out << "dialogue = " << choice.roomId << '/' << choice.nodeId << '/' << choice.choiceId
            << '\n';
    }
    return out.str();
}

SaveLoadResult SaveFiles::load(const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        if (ec) return {std::nullopt, "Nao foi possivel verificar " + TextFormat::pathToUtf8(path)};
        return {};
    }
    TextFormat::TextLoad text = TextFormat::readFile(path);
    if (!text.text) return {std::nullopt, std::move(text.error)};
    return parse(*text.text);
}

bool SaveFiles::save(const std::filesystem::path& path, const SavedGame& game,
                     std::string& error) {
    if (!validIdentifier(game.roomId)) {
        error = "Sala invalida: " + game.roomId;
        return false;
    }
    error = game.session.validationError();
    if (!error.empty()) return false;
    return TextFormat::writeTextAtomic(path, serialize(game), "save", error);
}

bool SaveFiles::quarantine(const std::filesystem::path& path, std::string& error) {
    auto bad = path;
    bad += ".bad";
    std::error_code ec;
    std::filesystem::rename(path, bad, ec);
    if (ec) {
        error = "Nao foi possivel renomear " + TextFormat::pathToUtf8(path.filename()) + " para "
                + TextFormat::pathToUtf8(bad.filename());
        return false;
    }
    return true;
}

std::filesystem::path SaveFiles::defaultSlot(std::string& error) {
    char* folder = SDL_GetPrefPath(organization, application);
    if (!folder) {
        error = SDL_GetError();
        return {};
    }
    // UTF-8 from SDL, with a trailing separator; ours to free.
    const std::u8string utf8(folder, folder + SDL_strlen(folder));
    SDL_free(folder);
    return std::filesystem::path(utf8) / slotName;
}

ResumeResult SaveFiles::resume(const std::filesystem::path& path, const RoomLibrary& library,
                               const RefusedSave refused, const std::string_view fallbackRoom) {
    ResumeResult out;
    SaveLoadResult loaded = load(path);
    if (loaded.save) {
        RoomBundleResult room = library.loadBundle(loaded.save->roomId + ".room");
        if (room.bundle) {
            out.save = std::move(loaded.save);
            out.bundle = std::move(room.bundle);
            return out;
        }
        std::string reason = "Sala " + loaded.save->roomId + ": " + room.error;
        RoomBundleResult start = library.loadBundle(fallbackRoom);
        if (start.bundle) {
            // The feet belonged to the other room: enter the start room as a new game would.
            loaded.save->roomId = start.bundle->room.id;
            loaded.save->feet = start.bundle->room.playerStart;
            loaded.save->facing = Facing::down;
            out.save = std::move(loaded.save);
            out.bundle = std::move(start.bundle);
            out.warning = std::move(reason);
            return out;
        }
        out.error = reason + "; " + std::string(fallbackRoom) + ": " + start.error;
    } else if (loaded.error.empty()) {
        return out; // no save yet
    } else {
        out.error = std::move(loaded.error);
    }
    if (refused == RefusedSave::keep) return out; // the file stays exactly as it is
    if (std::string renameError; !quarantine(path, renameError)) {
        out.error += " (" + renameError + ")";
    }
    return out;
}
