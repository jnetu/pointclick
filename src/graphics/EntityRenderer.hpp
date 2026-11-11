#pragma once

#include <SDL3/SDL_rect.h>

struct SDL_Renderer;
class SpriteLibrary;
class Player;
class AnimationPlayback;
struct Room;
struct SceneObject;

namespace EntityRenderer {
// With a playback: its clip. Without one: the placeholder, a flat rectangle in the object's color
// with an outline. The collision base is diagnostics and is drawn by debug/DebugOverlay only.
void drawObject(SDL_Renderer* renderer, SpriteLibrary& sprites,
                const SceneObject& object, const AnimationPlayback* playback,
                SDL_FPoint cameraOffset = {}, float interpolation = 1.0F);
// Where the player's sprite lands on screen (its current clip, or the room's base size when it
// has none); the speech is anchored to its top.
[[nodiscard]] SDL_FRect playerRect(const Room& room, const Player& player, SDL_FPoint feet,
                                   SDL_FPoint cameraOffset = {});
void drawPlayer(SpriteLibrary& sprites, const Room& room,
                const Player& player, SDL_FPoint feet,
                SDL_FPoint cameraOffset = {}, float interpolation = 1.0F);
}
