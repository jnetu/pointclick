#include "content/RoomFiles.hpp"
#include "content/ObjectFields.hpp"
#include "content/SpriteFields.hpp"
#include "content/TextFormat.hpp"
#include "game/Identifier.hpp"
#include "game/RoomParameters.hpp"

#include <locale>
#include <map>
#include <optional>
#include <ostream>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using RoomFieldValues::numbers;
using TextFormat::number;
using TextFormat::pathToUtf8;
using TextFormat::trim;

struct ColorField { const char* key; SDL_Color Room::* member; };
constexpr ColorField colors[]{
    {"wall.color", &Room::wallColor}, {"floor.color", &Room::floorColor},
    {"wall.trim_color", &Room::wallTrimColor}, {"floor.guide_color", &Room::floorGuideColor},
    {"player.color", &Room::playerColor}, {"player.outline_color", &Room::playerOutlineColor},
};

bool parsePoint(const std::string& text, SDL_FPoint& point) {
    float values[2];
    if (!numbers(text, values)) return false;
    point = {values[0], values[1]};
    return true;
}

std::optional<LayerPlane> parsePlane(const std::string_view text) {
    if (text == "back") return LayerPlane::back;
    if (text == "front") return LayerPlane::front;
    return std::nullopt;
}

// The keys of a [layer] block besides image.*; plain data, validated by Room::validationIssue.
bool parseLayerField(RoomLayer& layer, const std::string& key, const std::string& value) {
    if (key == "plane") {
        const auto plane = parsePlane(value);
        if (plane) layer.plane = *plane;
        return plane.has_value();
    }
    if (key == "position") return parsePoint(value, layer.position);
    if (key == "size") return parsePoint(value, layer.size);
    if (key == "scroll_factor") return parsePoint(value, layer.scrollFactor);
    if (key.starts_with("image.")) {
        return SpriteFields::parse(layer.image, std::string_view(key).substr(6), value);
    }
    return false;
}

void writePoint(std::ostream& out, const std::string_view key, const SDL_FPoint point) {
    out << key << " = " << number(point.x) << ", " << number(point.y) << '\n';
}

// Only the keys a designer changed from the defaults (D6: a full-room backdrop is image.file).
void writeLayer(std::ostream& out, const RoomLayer& layer) {
    const RoomLayer defaults;
    out << "\n[layer " << layer.id << "]\n";
    if (layer.plane != defaults.plane) out << "plane = front\n";
    SpriteFields::write(out, "image.", layer.image, SpriteFields::ClipKeys::changed);
    const auto same = [](const SDL_FPoint a, const SDL_FPoint b) {
        return a.x == b.x && a.y == b.y;
    };
    if (!same(layer.position, defaults.position)) writePoint(out, "position", layer.position);
    if (!same(layer.size, defaults.size)) writePoint(out, "size", layer.size);
    if (!same(layer.scrollFactor, defaults.scrollFactor)) {
        writePoint(out, "scroll_factor", layer.scrollFactor);
    }
}
}

RoomLoadResult RoomFiles::load(const std::filesystem::path& path,
                              const AnimationSet* inheritedPlayerArt) {
    auto loaded = TextFormat::readFile(path);
    if (!loaded.text) return {{}, std::move(loaded.error)};
    std::istringstream input(std::move(*loaded.text));
    // Defaults come from Room itself; art never comes from C++ (tests/support/RoomPresets is test-only).
    Room room;
    // A file without an `id` key is the room named after it, never Room's default "first".
    room.id = pathToUtf8(path.stem());
    if (inheritedPlayerArt) room.playerAnimations = *inheritedPlayerArt;
    SceneObject* object = nullptr;
    // Index, not pointer: spawns and layers are vectors that grow with each header.
    std::optional<std::size_t> spawn;
    std::optional<std::size_t> layer;
    std::string section = "room";
    // Where each key and each [spawn]/[layer]/[object] header was declared: Room::validationIssue stays
    // the single source of the rules and this only turns its answer into a line number.
    std::map<std::string, std::size_t> keys;
    std::vector<std::size_t> spawnLines;
    std::vector<std::size_t> objectLines;
    std::vector<std::size_t> layerLines;
    std::size_t lineNumber = 0;
    const auto failAt = [](const std::size_t line, const std::string& reason) -> RoomLoadResult {
        return {{}, "Linha " + std::to_string(line) + ": " + reason};
    };
    const auto fail = [&](const std::string& reason) { return failAt(lineNumber, reason); };
    for (std::string line; std::getline(input, line);) {
        ++lineNumber;
        line = trim(line.substr(0, line.find('#')));
        if (line.empty()) continue;
        if (line == "[room]") {
            object = nullptr;
            spawn.reset();
            layer.reset();
            section = "room";
            continue;
        }
        if (line.starts_with("[spawn ") && line.ends_with(']')) {
            const auto id = trim(line.substr(7, line.size() - 8));
            if (!validIdentifier(id) || room.findSpawn(id))
                return fail(std::string(Room::spawnIdError));
            object = nullptr;
            layer.reset();
            spawn = room.spawns.size();
            spawnLines.push_back(lineNumber);
            room.spawns.push_back({id, {}, Facing::down});
            section = "spawn." + id;
            continue;
        }
        if (line.starts_with("[layer ") && line.ends_with(']')) {
            const auto id = trim(line.substr(7, line.size() - 8));
            if (!validIdentifier(id) || room.findLayer(id))
                return fail(std::string(Room::layerIdError));
            object = nullptr;
            spawn.reset();
            layer = room.layers.size();
            layerLines.push_back(lineNumber);
            room.layers.push_back({});
            room.layers.back().id = id;
            section = "layer." + id;
            continue;
        }
        if (line.starts_with("[object ") && line.ends_with(']')) {
            spawn.reset();
            layer.reset();
            const auto id = trim(line.substr(8, line.size() - 9));
            if (!validIdentifier(id) || room.findObject(id))
                return fail(std::string(Room::objectIdError));
            objectLines.push_back(lineNumber);
            room.scenery.push_back({{}, {180, 180, 180, 255}, SceneObjectType::decoration,
                                    {}, id, {}, "idle"});
            object = &room.scenery.back();
            section = "object." + id;
            continue;
        }
        const auto equal = line.find('=');
        if (equal == std::string::npos) return fail("Esperado chave = valor");
        const auto key = trim(line.substr(0, equal));
        const auto value = trim(line.substr(equal + 1));
        const std::string canonicalKey = object && key.starts_with("sprite.")
                                         ? "animation.idle." + key.substr(7) : key;
        if (!keys.emplace(section + ":" + canonicalKey, lineNumber).second) {
            return fail("Chave repetida: " + key);
        }
        bool parsed = false;
        if (spawn) {
            RoomSpawn& target = room.spawns[*spawn];
            if (key == "feet") {
                parsed = parsePoint(value, target.feet);
            } else if (key == "facing") {
                const auto facing = parseFacing(value);
                parsed = facing.has_value();
                if (parsed) target.facing = *facing;
            }
        } else if (layer) {
            parsed = parseLayerField(room.layers[*layer], key, value);
        } else if (object) {
            if (const ObjectField* field = ObjectFields::find(key)) {
                parsed = field->read(value, *object);
            } else if (key.starts_with("sprite.")) {
                // Existing rooms used one unnamed sprite; it is the idle clip.
                parsed = SpriteFields::parse(object->animations["idle"],
                                             std::string_view(key).substr(7), value);
            } else if (key.starts_with("animation.")) {
                parsed = SpriteFields::parseNamed(object->animations, key, "animation.", value);
            }
        } else if (key == "id") {
            room.id = value;
            parsed = true;
        } else if (const auto* parameter = findRoomParameter(key)) {
            float number[1];
            parsed = numbers(value, number);
            if (parsed) {
                if (number[0] < parameter->minimum || number[0] > parameter->maximum) {
                    return fail("Fora do intervalo: " + key);
                }
                parameter->write(room, number[0]);
            }
        } else if (key == "floor.lines") {
            parsed = numbers(value, room.floorGuideFractions);
        } else {
            for (const auto& color : colors) {
                if (key == color.key) parsed = RoomFieldValues::color(value, room.*(color.member));
            }
            if (!parsed && key.starts_with("player.")) {
                parsed = SpriteFields::parseNamed(room.playerAnimations, key, "player.", value);
            }
        }
        if (!parsed) return fail("Chave ou valor invalido: " + key);
    }
    if (input.bad()) return {{}, "Falha de leitura"};
    for (std::size_t index = 0; index < room.spawns.size(); ++index) {
        const RoomSpawn& entry = room.spawns[index];
        if (!keys.contains("spawn." + entry.id + ":feet")) {
            return failAt(spawnLines[index], "Spawn sem feet: " + entry.id);
        }
    }
    if (const auto issue = room.validationIssue(); !issue.message.empty()) {
        using Subject = RoomValidationIssue::Subject;
        if (issue.subject == Subject::room) return {{}, issue.message};
        // The key that fed the error when the file has it; the block header otherwise.
        const auto [block, header] = [&]() -> std::pair<std::string, std::size_t> {
            switch (issue.subject) {
            case Subject::spawn:
                return {"spawn." + room.spawns[issue.index].id, spawnLines[issue.index]};
            case Subject::layer:
                return {"layer." + room.layers[issue.index].id, layerLines[issue.index]};
            case Subject::object:
            case Subject::room:
                break;
            }
            return {"object." + room.scenery[issue.index].id, objectLines[issue.index]};
        }();
        const auto declared = keys.find(block + ":" + std::string(issue.field));
        return failAt(declared != keys.end() ? declared->second : header, issue.message);
    }
    return {std::move(room), {}};
}

bool RoomFiles::save(const std::filesystem::path& path, const Room& room, std::string& error,
                     const AnimationSet* inheritedPlayerArt) {
    error = room.validationError();
    if (!error.empty()) return false;
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "# PointClick: coordenadas logicas; cores RGBA; retangulos x,y,w,h\n[room]\nid = " << room.id << '\n';
    for (const auto& parameter : roomParameters()) {
        out << parameter.key << " = " << number(parameter.read(room)) << '\n';
    }
    for (const auto& color : colors) {
        out << color.key << " = ";
        RoomFieldValues::writeColor(out, room.*(color.member));
        out << '\n';
    }
    for (const auto& [name, clip] : room.playerAnimations) {
        if (inheritedPlayerArt) {
            if (const SpriteClip* shared = findAnimation(*inheritedPlayerArt, name);
                shared && *shared == clip) {
                continue;
            }
        }
        SpriteFields::write(out, "player." + name + ".", clip);
    }
    out << "floor.lines = ";
    for (std::size_t i = 0; i < room.floorGuideFractions.size(); ++i) {
        out << (i == 0 ? "" : ", ") << number(room.floorGuideFractions[i]);
    }
    out << '\n';
    for (const RoomSpawn& entry : room.spawns) {
        out << "\n[spawn " << entry.id << "]\nfeet = " << number(entry.feet.x) << ", "
            << number(entry.feet.y) << "\nfacing = " << facingName(entry.facing) << '\n';
    }
    // Layers between the spawns and the objects, in file order (it is the drawing order).
    for (const RoomLayer& entry : room.layers) writeLayer(out, entry);
    for (const auto& object : room.scenery) {
        out << "\n[object " << object.id << "]\n";
        ObjectFields::write(out, object);
        for (const auto& [name, clip] : object.animations) {
            SpriteFields::write(out, "animation." + name + ".", clip);
        }
    }
    return TextFormat::writeTextAtomic(path, out.str(), "sala", error);
}
