#include "game/RoomParameters.hpp"
#include "game/Room.hpp"

namespace {
// Expressions also support components such as playerBaseSize.x.
#define PARAM(key, description, field, low, high, step) \
    {key, description, low, high, step, \
     [](const Room& room) { return room.field; }, \
     [](Room& room, float value) { room.field = value; }}

const RoomParameter parameters[]{
    PARAM("player.far_speed", "Velocidade no fundo (px/s)", playerFarSpeed, 0, 1000, 10),
    PARAM("player.near_speed", "Velocidade na frente (px/s)", playerNearSpeed, 0, 1000, 10),
    PARAM("player.width", "Largura base do personagem", playerBaseSize.x, 1, 400, 2),
    PARAM("player.height", "Altura base do personagem", playerBaseSize.y, 1, 600, 2),
    PARAM("player.start_x", "Posicao inicial X da sala", playerStart.x, 0, 1279, 10),
    PARAM("player.start_y", "Posicao inicial Y da sala", playerStart.y, 0, 719, 10),
    PARAM("perspective.far_scale", "Escala no fundo", farScale, 0.1F, 4, 0.05F),
    PARAM("perspective.near_scale", "Escala na frente", nearScale, 0.1F, 4, 0.05F),
    PARAM("perspective.far_y", "Y do fundo da perspectiva", farDepthY, 0, 719, 10),
    PARAM("perspective.near_y", "Y da frente da perspectiva", nearDepthY, 1, 720, 10),
    PARAM("room.wall_y", "Divisao entre parede e chao", wallBottomY, 0, 718, 10),
    PARAM("room.walkable_y", "Limite superior para os pes", walkableTopY, 0, 719, 10),
    PARAM("room.vanishing_x", "Ponto de fuga horizontal", vanishingPointX, 0, 1280, 10),
    PARAM("collision.padding_x", "Margem horizontal dos obstaculos", collisionPaddingX, 0, 200, 2),
    PARAM("collision.padding_y", "Margem vertical dos obstaculos", collisionPaddingY, 0, 200, 2),
    {"room.ray_spacing", "Espacamento das linhas do chao", 16, 1280, 16,
     [](const Room& room) { return static_cast<float>(room.floorRaySpacing); },
     [](Room& room, float value) { room.floorRaySpacing = static_cast<int>(value); }},
};
#undef PARAM
}

std::span<const RoomParameter> roomParameters() { return parameters; }

const RoomParameter* findRoomParameter(const std::string_view key) {
    for (const auto& parameter : parameters) {
        if (parameter.key == key) {
            return &parameter;
        }
    }
    return nullptr;
}
