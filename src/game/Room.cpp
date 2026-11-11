#include "game/Room.hpp"

#include "game/PlayerClips.hpp"
#include "game/World.hpp"
#include "game/RoomParameters.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <utility>

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
    const float scaledWidth = playerBaseSize.x * scale;
    const float scaledHeight = playerBaseSize.y * scale;
    return {feet.x - scaledWidth * 0.5F, feet.y - scaledHeight, scaledWidth, scaledHeight};
}

bool Room::containsFloor(const SDL_FPoint point) const {
    return point.x >= 0.0F && point.x < width
           && point.y >= wallBottomY && point.y < height;
}

SDL_FPoint Room::clampDestination(const SDL_FPoint point) const {
    const float y = std::clamp(point.y, walkableTopY, height - 1.0F);
    const float halfWidth = playerBaseSize.x * scaleAt(y) * 0.5F;
    return {std::clamp(point.x, halfWidth, width - halfWidth), y};
}

const SceneObject* Room::findObject(const std::string_view objectId) const {
    const auto found = std::find_if(scenery.begin(), scenery.end(),
                                    [&](const SceneObject& object) { return object.id == objectId; });
    return found == scenery.end() ? nullptr : &(*found);
}

const RoomSpawn* Room::findSpawn(const std::string_view spawnId) const {
    const auto found = std::find_if(spawns.begin(), spawns.end(),
                                    [&](const RoomSpawn& spawn) { return spawn.id == spawnId; });
    return found == spawns.end() ? nullptr : &(*found);
}

std::optional<RoomSpawn> Room::entryPoint(const std::string_view spawnId) const {
    if (spawnId.empty()) return RoomSpawn{{}, playerStart, Facing::down};
    if (const RoomSpawn* spawn = findSpawn(spawnId)) return *spawn;
    return std::nullopt;
}

RoomValidationIssue Room::validationIssue() const {
    using Subject = RoomValidationIssue::Subject;
    const auto issue = [](std::string message) { return RoomValidationIssue{std::move(message)}; };
    if (!validIdentifier(id)) return issue("ID da sala invalido (use letras, numeros, _ ou -)");
    for (const auto& parameter : roomParameters()) {
        const float value = parameter.read(*this);
        if (!std::isfinite(value) || value < parameter.minimum || value > parameter.maximum) {
            return issue("Fora do intervalo: " + std::string(parameter.key));
        }
    }
    if (farDepthY >= nearDepthY) return issue("far_y deve ser menor que near_y");
    if (nearDepthY > height) return issue("near_y nao pode ultrapassar a altura da sala");
    if (wallBottomY > walkableTopY) return issue("walkable_y deve ficar abaixo da parede");
    if (wallBottomY > height - 2.0F || walkableTopY > height - 1.0F) {
        return issue("Parede e chao caminhavel devem caber na altura da sala");
    }
    if (vanishingPointX < 0.0F || vanishingPointX > width) {
        return issue("vanishing_x fora da largura da sala");
    }
    if (playerStart.x < 0.0F || playerStart.x >= width
        || playerStart.y < 0.0F || playerStart.y >= height) {
        return issue("Posicao inicial do player fora da sala");
    }
    std::set<std::string_view> spawnIds;
    for (std::size_t index = 0; index < spawns.size(); ++index) {
        const RoomSpawn& spawn = spawns[index];
        if (!validIdentifier(spawn.id) || !spawnIds.insert(spawn.id).second) {
            return {std::string(spawnIdError), Subject::spawn, index, {}};
        }
        if (!std::isfinite(spawn.feet.x) || !std::isfinite(spawn.feet.y) || spawn.feet.x < 0.0F
            || spawn.feet.x >= width || spawn.feet.y < 0.0F || spawn.feet.y >= height) {
            return {"Spawn fora da sala: " + spawn.id, Subject::spawn, index, "feet"};
        }
    }
    const float largestScale = std::max(farScale, nearScale);
    if (playerBaseSize.x * largestScale >= World::width
        || playerBaseSize.y * largestScale >= World::height) {
        return issue("O tamanho escalado do player deve caber na tela");
    }
    for (const float fraction : floorGuideFractions) {
        if (!std::isfinite(fraction) || fraction < 0 || fraction > 1)
            return issue("Linhas do chao: use 0 a 1");
    }
    for (const std::string_view name : playerLocomotionAnimations) {
        if (!findAnimation(playerAnimations, name))
            return issue("Animacao obrigatoria do player: " + std::string(name));
    }
    for (const auto& [name, clip] : playerAnimations) {
        if (!validIdentifier(name)) return issue("Nome de animacao do player invalido: " + name);
        if (const auto problem = clip.validationError(); !problem.empty()) {
            return issue("Animacao do player " + name + ": " + problem);
        }
    }
    const auto validRect = [](const SDL_FRect r) {
        return std::isfinite(r.x) && std::isfinite(r.y) && std::isfinite(r.w)
               && std::isfinite(r.h) && r.w > 0 && r.h > 0;
    };
    std::set<std::string> ids;
    for (std::size_t index = 0; index < scenery.size(); ++index) {
        const SceneObject& object = scenery[index];
        const auto objectIssue = [&](std::string message, const std::string_view field = {}) {
            return RoomValidationIssue{std::move(message), Subject::object, index, field};
        };
        if (!validIdentifier(object.id) || !ids.insert(object.id).second) {
            return objectIssue(std::string(objectIdError));
        }
        if (!validRect(object.bounds) || object.bounds.x < 0 || object.bounds.y < 0
            || object.bounds.x + object.bounds.w > width
            || object.bounds.y + object.bounds.h > height) {
            return objectIssue("Objeto fora da sala: " + object.id, "bounds");
        }
        if (!validIdentifier(object.initialAnimation)) {
            return objectIssue("Animacao inicial invalida: " + object.id, "animation.initial");
        }
        if (!object.animations.empty() && !findAnimation(object.animations, object.initialAnimation)) {
            return objectIssue("Animacao inicial ausente: " + object.id, "animation.initial");
        }
        for (const auto& [name, clip] : object.animations) {
            if (!validIdentifier(name))
                return objectIssue("Nome de animacao invalido: " + object.id);
            if (const auto problem = clip.validationError(); !problem.empty()) {
                return objectIssue("Animacao de " + object.id + " (" + name + "): " + problem);
            }
        }
        const auto base = object.collisionFootprint;
        if (!std::isfinite(base.x) || !std::isfinite(base.y) || !std::isfinite(base.w)
            || !std::isfinite(base.h)) {
            return objectIssue("Base de colisao invalida: " + object.id, "collision");
        }
        if (object.type == SceneObjectType::solid || base.w != 0 || base.h != 0) {
            if (!validRect(base) || base.x < 0 || base.y < 0
                || base.x + base.w > object.bounds.w || base.y + base.h > object.bounds.h) {
                return objectIssue("Base de colisao invalida: " + object.id, "collision");
            }
        }
    }
    return {};
}

std::string Room::validationError() const {
    return validationIssue().message;
}
