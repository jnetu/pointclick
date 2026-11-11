#pragma once

struct SDL_Renderer;
class Game;
class DialogueMenu;
class TextRenderer;

// Draws the open dialogue choices in viewport space, using DialogueMenu's layout. Read only.
namespace DialogueMenuRenderer {
void draw(SDL_Renderer* renderer, TextRenderer& text, const Game& game,
          const DialogueMenu& menu);
}
