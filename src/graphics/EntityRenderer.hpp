#pragma once

#include <SDL3/SDL_rect.h>

struct SDL_Renderer;
class SpriteLibrary;
class Player;
class AnimationPlayback;
struct Room;
struct SceneObject;

namespace EntityRenderer {
void drawObject(SDL_Renderer* renderer, SpriteLibrary& sprites,
                const SceneObject& object, const AnimationPlayback* playback);
void drawPlayer(SpriteLibrary& sprites, const Room& room,
                const Player& player, SDL_FPoint feet);
}
