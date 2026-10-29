#include "graphics/EntityRenderer.hpp"

#include "game/Player.hpp"
#include "game/Room.hpp"
#include "graphics/SpriteLibrary.hpp"

#include <SDL3/SDL.h>

void EntityRenderer::drawObject(SDL_Renderer* renderer, SpriteLibrary& sprites,
                                const SceneObject& object,
                                const AnimationPlayback* playback,
                                const SDL_FPoint cameraOffset) {
    if (playback) {
        const SpriteClip* clip = findAnimation(object.animations, playback->name());
        if (!clip) return;
        const SDL_FPoint foot{object.bounds.x + object.bounds.w * 0.5F - cameraOffset.x,
                              object.bounds.y + object.bounds.h - cameraOffset.y};
        (void)sprites.draw(*clip, clip->targetRect(foot, object.bounds.w, object.bounds.h),
                           playback->elapsed());
        return;
    }
    const SDL_FRect bounds{object.bounds.x - cameraOffset.x,
                           object.bounds.y - cameraOffset.y,
                           object.bounds.w, object.bounds.h};
    SDL_SetRenderDrawColor(renderer, object.color.r, object.color.g, object.color.b, 255);
    SDL_RenderFillRect(renderer, &bounds);
    if (object.type == SceneObjectType::solid) {
        SDL_FRect base = object.worldCollisionFootprint();
        base.x -= cameraOffset.x;
        base.y -= cameraOffset.y;
        SDL_SetRenderDrawColor(renderer, 130, 42, 39, 255);
        SDL_RenderFillRect(renderer, &base);
    }
    SDL_SetRenderDrawColor(renderer, 210, 218, 231, 255);
    SDL_RenderRect(renderer, &bounds);
}

void EntityRenderer::drawPlayer(SpriteLibrary& sprites, const Room& room,
                                const Player& player, const SDL_FPoint feet,
                                const SDL_FPoint cameraOffset) {
    const SpriteClip* clip = findAnimation(room.playerAnimations, player.animationName());
    if (!clip) return;
    const SDL_FPoint screenFeet{feet.x - cameraOffset.x, feet.y - cameraOffset.y};
    (void)sprites.draw(*clip, clip->targetRect(screenFeet, room.playerBaseSize.x,
                                               room.playerBaseSize.y, room.scaleAt(feet.y)),
                       player.animationTime());
}
