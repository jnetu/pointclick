#pragma once

#include "game/SceneObject.hpp"

#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_rect.h>

#include <iosfwd>
#include <span>
#include <string>
#include <string_view>

// Um campo escalar do bloco [object] do .room. Os clips (sprite.*, animation.<clip>.*) continuam
// em SpriteFields. Campo novo de objeto = uma entrada na tabela de ObjectFields.cpp.
struct ObjectField {
    std::string_view key;
    // Lê o texto depois do '='; false = valor inválido ("Chave ou valor invalido: <chave>").
    bool (*read)(const std::string& value, SceneObject& object);
    // Grava só o valor, sem a chave nem o fim de linha.
    void (*write)(std::ostream& out, const SceneObject& object);
    // nullptr = sempre gravado; senão o campo fica fora do arquivo quando devolve true.
    bool (*omitted)(const SceneObject& object) = nullptr;
};

namespace ObjectFields {
// Na ordem em que RoomFiles::save grava.
[[nodiscard]] std::span<const ObjectField> fields();
[[nodiscard]] const ObjectField* find(std::string_view key);
// "<chave> = <valor>\n" de cada campo não omitido, na ordem da tabela.
void write(std::ostream& out, const SceneObject& object);
}

// Valores do .room que a sala e os objetos compartilham: números com ',' ou espaço, cor RGBA
// inteira em 0..255, retângulo x,y,w,h. Todo float gravado passa por TextFormat::number.
namespace RoomFieldValues {
[[nodiscard]] bool numbers(std::string text, std::span<float> values);
[[nodiscard]] bool color(const std::string& text, SDL_Color& color);
[[nodiscard]] bool rect(const std::string& text, SDL_FRect& rect);
void writeColor(std::ostream& out, SDL_Color color);
void writeRect(std::ostream& out, SDL_FRect rect);
}
