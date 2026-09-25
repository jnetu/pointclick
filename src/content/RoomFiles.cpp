#include "content/RoomFiles.hpp"
#include "content/RoomParameters.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <set>
#include <sstream>

namespace {
std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}

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

bool parseRect(const std::string& text, SDL_FRect& rect) {
    float values[4];
    if (!numbers(text, values)) return false;
    rect = {values[0], values[1], values[2], values[3]};
    return true;
}

bool parseSpriteField(SpriteClip& clip, const std::string_view key,
                      const std::string& value) {
    if (key == "file") { clip.file = value; return true; }
    if (key == "loop" || key == "pixelated") {
        if (value != "0" && value != "1") return false;
        (key == "loop" ? clip.loop : clip.pixelated) = value == "1";
        return true;
    }
    float number[1];
    if (!numbers(value, number)) return false;
    const auto integer = [&](int& field) {
        if (std::floor(number[0]) != number[0]
            || number[0] < -8192 || number[0] > 8192) return false;
        field = static_cast<int>(number[0]);
        return true;
    };
    if (key == "frame_width") return integer(clip.frameWidth);
    if (key == "frame_height") return integer(clip.frameHeight);
    if (key == "frames") return integer(clip.frames);
    if (key == "columns") return integer(clip.columns);
    if (key == "first_frame") return integer(clip.firstFrame);
    if (key == "margin_x") return integer(clip.marginX);
    if (key == "margin_y") return integer(clip.marginY);
    if (key == "spacing_x") return integer(clip.spacingX);
    if (key == "spacing_y") return integer(clip.spacingY);
    if (key == "fps") { clip.fps = number[0]; return true; }
    if (key == "display_width") { clip.displayWidth = number[0]; return true; }
    if (key == "display_height") { clip.displayHeight = number[0]; return true; }
    if (key == "offset_x") { clip.offsetX = number[0]; return true; }
    if (key == "offset_y") { clip.offsetY = number[0]; return true; }
    return false;
}

void writeSprite(std::ostream& out, const std::string_view prefix, const SpriteClip& clip) {
    out << prefix << "file = " << clip.file << '\n';
    out << prefix << "frame_width = " << clip.frameWidth << '\n';
    out << prefix << "frame_height = " << clip.frameHeight << '\n';
    out << prefix << "frames = " << clip.frames << '\n';
    out << prefix << "columns = " << clip.columns << '\n';
    out << prefix << "first_frame = " << clip.firstFrame << '\n';
    out << prefix << "margin_x = " << clip.marginX << '\n';
    out << prefix << "margin_y = " << clip.marginY << '\n';
    out << prefix << "spacing_x = " << clip.spacingX << '\n';
    out << prefix << "spacing_y = " << clip.spacingY << '\n';
    out << prefix << "fps = " << clip.fps << '\n';
    out << prefix << "loop = " << (clip.loop ? 1 : 0) << '\n';
    out << prefix << "pixelated = " << (clip.pixelated ? 1 : 0) << '\n';
    out << prefix << "display_width = " << clip.displayWidth << '\n';
    out << prefix << "display_height = " << clip.displayHeight << '\n';
    out << prefix << "offset_x = " << clip.offsetX << '\n';
    out << prefix << "offset_y = " << clip.offsetY << '\n';
}

SpriteClip* playerSpriteFor(Room& room, const std::string_view key,
                            std::string_view& field) {
    constexpr std::string_view idle = "player.idle.";
    constexpr std::string_view left = "player.left.";
    constexpr std::string_view right = "player.right.";
    constexpr std::string_view up = "player.up.";
    constexpr std::string_view down = "player.down.";
    if (key.starts_with(idle)) { field = key.substr(idle.size()); return &room.playerSprites.idle; }
    if (key.starts_with(left)) { field = key.substr(left.size()); return &room.playerSprites.left; }
    if (key.starts_with(right)) { field = key.substr(right.size()); return &room.playerSprites.right; }
    if (key.starts_with(up)) { field = key.substr(up.size()); return &room.playerSprites.up; }
    if (key.starts_with(down)) { field = key.substr(down.size()); return &room.playerSprites.down; }
    return nullptr;
}

void writeColor(std::ostream& out, const SDL_Color color) {
    out << static_cast<int>(color.r) << ", " << static_cast<int>(color.g) << ", "
        << static_cast<int>(color.b) << ", " << static_cast<int>(color.a) << '\n';
}

void writeRect(std::ostream& out, const SDL_FRect rect) {
    out << rect.x << ", " << rect.y << ", " << rect.w << ", " << rect.h << '\n';
}
}

RoomLoadResult RoomFiles::load(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) return {{}, "Nao foi possivel abrir " + path.filename().string()};
    Room room;
    SceneObject* object = nullptr;
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
        if (line == "[room]") { object = nullptr; section = "room"; continue; }
        if (line.starts_with("[object ") && line.ends_with(']')) {
            const auto id = trim(line.substr(8, line.size() - 9));
            room.scenery.push_back({{}, {180, 180, 180, 255}, SceneObjectType::decoration, {}, id, {}});
            object = &room.scenery.back();
            section = "object." + id;
            continue;
        }
        const auto equal = line.find('=');
        if (equal == std::string::npos) return fail("Esperado chave = valor");
        const auto key = trim(line.substr(0, equal));
        const auto value = trim(line.substr(equal + 1));
        if (!keys.insert(section + ":" + key).second) return fail("Chave repetida: " + key);
        bool parsed = false;
        if (object) {
            if (key == "bounds") parsed = parseRect(value, object->bounds);
            else if (key == "collision") parsed = parseRect(value, object->collisionFootprint);
            else if (key == "color") parsed = parseColor(value, object->color);
            else if (key == "type" && (value == "solid" || value == "decoration")) {
                object->type = value == "solid" ? SceneObjectType::solid : SceneObjectType::decoration;
                parsed = true;
            } else if (key.starts_with("sprite.")) {
                if (!object->sprite) object->sprite.emplace();
                parsed = parseSpriteField(*object->sprite, std::string_view(key).substr(7), value);
            }
        } else if (key == "id") {
            room.id = value;
            parsed = true;
        } else if (std::string_view spriteField; auto* clip = playerSpriteFor(room, key, spriteField)) {
            parsed = parseSpriteField(*clip, spriteField, value);
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
        }
        if (!parsed) return fail("Chave ou valor invalido: " + key);
    }
    if (input.bad()) return {{}, "Falha de leitura"};
    if (const auto error = room.validationError(); !error.empty()) return {{}, error};
    return {std::move(room), {}};
}

bool RoomFiles::save(const std::filesystem::path& path, const Room& room, std::string& error) {
    error = room.validationError();
    if (!error.empty()) return false;
    const auto temporary = std::filesystem::path(path.string() + ".tmp");
    std::ofstream out(temporary);
    if (!out) { error = "Nao foi possivel salvar " + path.filename().string(); return false; }
    out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<float>::max_digits10);
    out << "# PointClick: coordenadas logicas; cores RGBA; retangulos x,y,w,h\n[room]\nid = " << room.id << '\n';
    for (const auto& parameter : roomParameters()) {
        out << parameter.key << " = " << parameter.read(room) << '\n';
    }
    for (const auto& color : colors) { out << color.key << " = "; writeColor(out, room.*(color.member)); }
    writeSprite(out, "player.idle.", room.playerSprites.idle);
    writeSprite(out, "player.left.", room.playerSprites.left);
    writeSprite(out, "player.right.", room.playerSprites.right);
    writeSprite(out, "player.up.", room.playerSprites.up);
    writeSprite(out, "player.down.", room.playerSprites.down);
    out << "floor.lines = ";
    for (std::size_t i = 0; i < room.floorGuideFractions.size(); ++i) {
        out << (i == 0 ? "" : ", ") << room.floorGuideFractions[i];
    }
    out << '\n';
    for (const auto& object : room.scenery) {
        out << "\n[object " << object.id << "]\nbounds = "; writeRect(out, object.bounds);
        out << "color = "; writeColor(out, object.color);
        out << "type = " << (object.type == SceneObjectType::solid ? "solid" : "decoration") << '\n';
        out << "collision = "; writeRect(out, object.collisionFootprint);
        if (object.sprite) writeSprite(out, "sprite.", *object.sprite);
    }
    out.close();
    std::error_code ioError;
    if (out) std::filesystem::rename(temporary, path, ioError);
    if (!out || ioError) {
        std::filesystem::remove(temporary, ioError);
        error = "Falha ao gravar o arquivo de sala";
        return false;
    }
    return true;
}
