#include "graphics/EntityRenderer.hpp"

#include "game/Player.hpp"
#include "game/Room.hpp"
#include "graphics/SpriteLibrary.hpp"

#include <SDL3/SDL.h>

void EntityRenderer::drawObject(SDL_Renderer* renderer, SpriteLibrary& sprites,
                                const SceneObject& object,
                                const AnimationPlayback* playback) {
    if (playback) {
        const SpriteClip* clip = findAnimation(object.animations, playback->name());
        if (!clip) return;
        const SDL_FPoint foot{object.bounds.x + object.bounds.w * 0.5F,
                              object.bounds.y + object.bounds.h};
        (void)sprites.draw(*clip, clip->targetRect(foot, object.bounds.w, object.bounds.h),
                           playback->elapsed());
        return;
    }
    SDL_SetRenderDrawColor(renderer, object.color.r, object.color.g, object.color.b, 255);
    SDL_RenderFillRect(renderer, &object.bounds);
    if (object.type == SceneObjectType::solid) {
        const SDL_FRect base = object.worldCollisionFootprint();
        SDL_SetRenderDrawColor(renderer, 130, 42, 39, 255);
        SDL_RenderFillRect(renderer, &base);
    }
    SDL_SetRenderDrawColor(renderer, 210, 218, 231, 255);
    SDL_RenderRect(renderer, &object.bounds);
}

void EntityRenderer::drawPlayer(SpriteLibrary& sprites, const Room& room,
                                const Player& player, const SDL_FPoint feet) {
    const SpriteClip* clip = findAnimation(room.playerAnimations, player.animationName());
    if (!clip) return;
    (void)sprites.draw(*clip, clip->targetRect(feet, room.playerBaseSize.x,
                                               room.playerBaseSize.y, room.scaleAt(feet.y)),
                       player.animationTime());
}
