#include "content/RoomFiles.hpp"
#include "content/SpriteFields.hpp"
#include "content/TextFormat.hpp"
#include "game/RoomParameters.hpp"

#include <SDL3/SDL_stdinc.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <optional>
#include <set>
#include <sstream>

namespace {
using TextFormat::pathToUtf8;
using TextFormat::trim;

bool numbers(std::string text, const std::span<float> values) {
    std::replace(text.begin(), text.end(), ',', ' ');
    std::istringstream input(text);
    input.imbue(std::locale::classic());
    for (float& value : values) {
        if (!(input >> value) || !std::isfinite(value)) return false;
    }
    input >> std::ws;
    return input.eof();
}

struct ColorField { const char* key; SDL_Color Room::* member; };
constexpr ColorField colors[]{
    {"wall.color", &Room::wallColor}, {"floor.color", &Room::floorColor},
    {"wall.trim_color", &Room::wallTrimColor}, {"floor.guide_color", &Room::floorGuideColor},
    {"player.color", &Room::playerColor}, {"player.outline_color", &Room::playerOutlineColor},
};

bool parseColor(const std::string& text, SDL_Color& color) {
    float values[4];
    if (!numbers(text, values)) return false;
    for (float value : values) {
        if (value < 0 || value > 255 || std::floor(value) != value) return false;
    }
    color = {static_cast<Uint8>(values[0]), static_cast<Uint8>(values[1]),
             static_cast<Uint8>(values[2]), static_cast<Uint8>(values[3])};
    return true;
}

bool parsePoint(const std::string& text, SDL_FPoint& point) {
    float values[2];
    if (!numbers(text, values)) return false;
    point = {values[0], values[1]};
    return true;
}

bool parseRect(const std::string& text, SDL_FRect& rect) {
    float values[4];
    if (!numbers(text, values)) return false;
    rect = {values[0], values[1], values[2], values[3]};
    return true;
}


void writeColor(std::ostream& out, const SDL_Color color) {
    out << static_cast<int>(color.r) << ", " << static_cast<int>(color.g) << ", "
        << static_cast<int>(color.b) << ", " << static_cast<int>(color.a) << '\n';
}

void writeRect(std::ostream& out, const SDL_FRect rect) {
    out << rect.x << ", " << rect.y << ", " << rect.w << ", " << rect.h << '\n';
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
    // Index, not pointer: spawns is a vector that grows with each [spawn] header.
    std::optional<std::size_t> spawn;
    std::string section = "room";
    std::set<std::string> keys;
    std::size_t lineNumber = 0;
    const auto fail = [&](const std::string& reason) -> RoomLoadResult {
        return {{}, "Linha " + std::to_string(lineNumber) + ": " + reason};
    };
    for (std::string line; std::getline(input, line);) {
        ++lineNumber;
        line = trim(line.substr(0, line.find('#')));
        if (line.empty()) continue;
        if (line == "[room]") { object = nullptr; spawn.reset(); section = "room"; continue; }
        if (line.starts_with("[spawn ") && line.ends_with(']')) {
            const auto id = trim(line.substr(7, line.size() - 8));
            object = nullptr;
            spawn = room.spawns.size();
            room.spawns.push_back({id, {}, Facing::down});
            section = "spawn." + id;
            continue;
        }
        if (line.starts_with("[object ") && line.ends_with(']')) {
            spawn.reset();
            const auto id = trim(line.substr(8, line.size() - 9));
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
        if (!keys.insert(section + ":" + canonicalKey).second) {
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
        } else if (object) {
            if (key == "bounds") parsed = parseRect(value, object->bounds);
            else if (key == "collision") parsed = parseRect(value, object->collisionFootprint);
            else if (key == "color") parsed = parseColor(value, object->color);
            else if (key == "type" && (value == "solid" || value == "decoration")) {
                object->type = value == "solid" ? SceneObjectType::solid : SceneObjectType::decoration;
                parsed = true;
            } else if (key == "animation.initial") {
                object->initialAnimation = value;
                parsed = true;
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
                if (key == color.key) parsed = parseColor(value, room.*(color.member));
            }
            if (!parsed && key.starts_with("player.")) {
                parsed = SpriteFields::parseNamed(room.playerAnimations, key, "player.", value);
            }
        }
        if (!parsed) return fail("Chave ou valor invalido: " + key);
    }
    if (input.bad()) return {{}, "Falha de leitura"};
    for (const RoomSpawn& entry : room.spawns) {
        if (!keys.contains("spawn." + entry.id + ":feet")) {
            return {{}, "Spawn sem feet: " + entry.id};
        }
    }
    if (const auto error = room.validationError(); !error.empty()) return {{}, error};
    return {std::move(room), {}};
}

bool RoomFiles::save(const std::filesystem::path& path, const Room& room, std::string& error,
                     const AnimationSet* inheritedPlayerArt) {
    error = room.validationError();
    if (!error.empty()) return false;
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<float>::max_digits10);
    out << "# PointClick: coordenadas logicas; cores RGBA; retangulos x,y,w,h\n[room]\nid = " << room.id << '\n';
    for (const auto& parameter : roomParameters()) {
        out << parameter.key << " = " << parameter.read(room) << '\n';
    }
    for (const auto& color : colors) { out << color.key << " = "; writeColor(out, room.*(color.member)); }
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
        out << (i == 0 ? "" : ", ") << room.floorGuideFractions[i];
    }
    out << '\n';
    for (const RoomSpawn& entry : room.spawns) {
        out << "\n[spawn " << entry.id << "]\nfeet = " << entry.feet.x << ", " << entry.feet.y
            << "\nfacing = " << facingName(entry.facing) << '\n';
    }
    for (const auto& object : room.scenery) {
        out << "\n[object " << object.id << "]\nbounds = "; writeRect(out, object.bounds);
        out << "color = "; writeColor(out, object.color);
        out << "type = " << (object.type == SceneObjectType::solid ? "solid" : "decoration") << '\n';
        out << "collision = "; writeRect(out, object.collisionFootprint);
        if (!object.animations.empty()) {
            out << "animation.initial = " << object.initialAnimation << '\n';
            for (const auto& [name, clip] : object.animations) {
                SpriteFields::write(out, "animation." + name + ".", clip);
            }
        }
    }
    return TextFormat::writeTextAtomic(path, out.str(), "sala", error);
}
