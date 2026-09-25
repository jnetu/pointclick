#include "game/Room.hpp"

#include "game/World.hpp"
#include "game/Player.hpp"
#include "game/RoomParameters.hpp"

#include <algorithm>
#include <cmath>
#include <set>

float Room::depthAt(const float feetY) const {
    const float range = std::max(nearDepthY - farDepthY, 1.0F);
    return std::clamp((feetY - farDepthY) / range, 0.0F, 1.0F);
}

float Room::scaleAt(const float feetY) const {
    return std::lerp(farScale, nearScale, depthAt(feetY));
}

float Room::speedAt(const float feetY) const {
    return std::lerp(playerFarSpeed, playerNearSpeed, depthAt(feetY));
}

SDL_FRect Room::playerBounds(const SDL_FPoint feet) const {
    const float scale = scaleAt(feet.y);
    const float width = playerBaseSize.x * scale;
    const float height = playerBaseSize.y * scale;
    return {feet.x - width * 0.5F, feet.y - height, width, height};
}

bool Room::containsFloor(const SDL_FPoint point) const {
    return point.x >= 0.0F && point.x < World::width
           && point.y >= wallBottomY && point.y < World::height;
}

SDL_FPoint Room::clampDestination(const SDL_FPoint point) const {
    const float y = std::clamp(point.y, walkableTopY, static_cast<float>(World::height - 1));
    const float halfWidth = playerBaseSize.x * scaleAt(y) * 0.5F;
    return {std::clamp(point.x, halfWidth, World::width - halfWidth), y};
}

std::string Room::validationError() const {
    if (!validIdentifier(id)) return "ID da sala invalido (use letras, numeros, _ ou -)";
    for (const auto& parameter : roomParameters()) {
        const float value = parameter.read(*this);
        if (!std::isfinite(value) || value < parameter.minimum || value > parameter.maximum) {
            return "Fora do intervalo: " + std::string(parameter.key);
        }
    }
    if (farDepthY >= nearDepthY) return "far_y deve ser menor que near_y";
    if (wallBottomY > walkableTopY) return "walkable_y deve ficar abaixo da parede";
    const float largestScale = std::max(farScale, nearScale);
    if (playerBaseSize.x * largestScale >= World::width
        || playerBaseSize.y * largestScale >= World::height) {
        return "O tamanho escalado do player deve caber na tela";
    }
    for (const float fraction : floorGuideFractions) {
        if (!std::isfinite(fraction) || fraction < 0 || fraction > 1) return "Linhas do chao: use 0 a 1";
    }
    for (const std::string_view name : playerLocomotionAnimations) {
        if (!findAnimation(playerAnimations, name)) return "Animacao obrigatoria do player: " + std::string(name);
    }
    for (const auto& [name, clip] : playerAnimations) {
        if (!validIdentifier(name)) return "Nome de animacao do player invalido: " + name;
        if (const auto problem = clip.validationError(); !problem.empty()) {
            return "Animacao do player " + name + ": " + problem;
        }
    }
    const auto validRect = [](const SDL_FRect r) {
        return std::isfinite(r.x) && std::isfinite(r.y) && std::isfinite(r.w)
               && std::isfinite(r.h) && r.w > 0 && r.h > 0;
    };
    std::set<std::string> ids;
    for (const auto& object : scenery) {
        if (!validIdentifier(object.id) || !ids.insert(object.id).second) {
            return "ID de objeto invalido ou repetido";
        }
        if (!validRect(object.bounds) || object.bounds.x < 0 || object.bounds.y < 0
            || object.bounds.x + object.bounds.w > World::width
            || object.bounds.y + object.bounds.h > World::height) return "Objeto fora da tela: " + object.id;
        if (!validIdentifier(object.initialAnimation)) return "Animacao inicial invalida: " + object.id;
        if (!object.animations.empty() && !findAnimation(object.animations, object.initialAnimation)) {
            return "Animacao inicial ausente: " + object.id;
        }
        for (const auto& [name, clip] : object.animations) {
            if (!validIdentifier(name)) return "Nome de animacao invalido: " + object.id;
            if (const auto problem = clip.validationError(); !problem.empty()) {
                return "Animacao de " + object.id + " (" + name + "): " + problem;
            }
        }
        const auto base = object.collisionFootprint;
        if (!std::isfinite(base.x) || !std::isfinite(base.y) || !std::isfinite(base.w)
            || !std::isfinite(base.h)) return "Base de colisao invalida: " + object.id;
        if (object.type == SceneObjectType::solid || base.w != 0 || base.h != 0) {
            if (!validRect(base) || base.x < 0 || base.y < 0
                || base.x + base.w > object.bounds.w || base.y + base.h > object.bounds.h) {
                return "Base de colisao invalida: " + object.id;
            }
        }
    }
    return {};
}
