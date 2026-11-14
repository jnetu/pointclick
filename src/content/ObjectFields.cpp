#include "content/ObjectFields.hpp"

#include "content/TextFormat.hpp"

#include <SDL3/SDL_stdinc.h>

#include <algorithm>
#include <cmath>
#include <locale>
#include <ostream>
#include <sstream>

namespace {
using RoomFieldValues::writeColor;
using RoomFieldValues::writeRect;

constexpr ObjectField kFields[]{
    {"bounds",
     [](const std::string& value, SceneObject& object) {
         return RoomFieldValues::rect(value, object.bounds);
     },
     [](std::ostream& out, const SceneObject& object) { writeRect(out, object.bounds); }},
    {"color",
     [](const std::string& value, SceneObject& object) {
         return RoomFieldValues::color(value, object.color);
     },
     [](std::ostream& out, const SceneObject& object) { writeColor(out, object.color); }},
    {"type",
     [](const std::string& value, SceneObject& object) {
         if (value != "solid" && value != "decoration") return false;
         object.type = value == "solid" ? SceneObjectType::solid : SceneObjectType::decoration;
         return true;
     },
     [](std::ostream& out, const SceneObject& object) {
         out << (object.type == SceneObjectType::solid ? "solid" : "decoration");
     }},
    {"collision",
     [](const std::string& value, SceneObject& object) {
         return RoomFieldValues::rect(value, object.collisionFootprint);
     },
     [](std::ostream& out, const SceneObject& object) {
         writeRect(out, object.collisionFootprint);
     }},
    // Qualquer texto entra; Room::validationIssue recusa ID inválido ou clip ausente.
    {"animation.initial",
     [](const std::string& value, SceneObject& object) {
         object.initialAnimation = value;
         return true;
     },
     [](std::ostream& out, const SceneObject& object) { out << object.initialAnimation; },
     // Sem clips o objeto é o placeholder colorido: não há animação inicial a gravar.
     [](const SceneObject& object) { return object.animations.empty(); }},
};
}

std::span<const ObjectField> ObjectFields::fields() {
    return kFields;
}

const ObjectField* ObjectFields::find(const std::string_view key) {
    const auto found = std::ranges::find(kFields, key, &ObjectField::key);
    return found != std::ranges::end(kFields) ? &*found : nullptr;
}

void ObjectFields::write(std::ostream& out, const SceneObject& object) {
    for (const ObjectField& field : kFields) {
        if (field.omitted && field.omitted(object)) continue;
        out << field.key << " = ";
        field.write(out, object);
        out << '\n';
    }
}

bool RoomFieldValues::numbers(std::string text, const std::span<float> values) {
    std::replace(text.begin(), text.end(), ',', ' ');
    std::istringstream input(text);
    input.imbue(std::locale::classic());
    for (float& value : values) {
        if (!(input >> value) || !std::isfinite(value)) return false;
    }
    input >> std::ws;
    return input.eof();
}

bool RoomFieldValues::color(const std::string& text, SDL_Color& color) {
    float values[4];
    if (!numbers(text, values)) return false;
    for (float value : values) {
        if (value < 0 || value > 255 || std::floor(value) != value) return false;
    }
    color = {static_cast<Uint8>(values[0]), static_cast<Uint8>(values[1]),
             static_cast<Uint8>(values[2]), static_cast<Uint8>(values[3])};
    return true;
}

bool RoomFieldValues::rect(const std::string& text, SDL_FRect& rect) {
    float values[4];
    if (!numbers(text, values)) return false;
    rect = {values[0], values[1], values[2], values[3]};
    return true;
}

void RoomFieldValues::writeColor(std::ostream& out, const SDL_Color color) {
    out << static_cast<int>(color.r) << ", " << static_cast<int>(color.g) << ", "
        << static_cast<int>(color.b) << ", " << static_cast<int>(color.a);
}

void RoomFieldValues::writeRect(std::ostream& out, const SDL_FRect rect) {
    using TextFormat::number;
    out << number(rect.x) << ", " << number(rect.y) << ", " << number(rect.w) << ", "
        << number(rect.h);
}
