#pragma once

struct SDL_Renderer;
class Game;
class DialogueMenu;

// Draws the open dialogue choices in viewport space, using DialogueMenu's layout. Read only.
namespace DialogueMenuRenderer {
void draw(SDL_Renderer* renderer, const Game& game, const DialogueMenu& menu);
}
