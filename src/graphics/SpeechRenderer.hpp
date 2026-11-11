#pragma once

#include "ui/SpeechLayout.hpp"

class Scene;
class TextRenderer;

// Draws the line on screen (Scene::floatingText) above the player's head, using SpeechLayout for
// wrapping and placement. Keeps the layout between frames so wrapping runs once per line.
class SpeechRenderer {
public:
    void draw(TextRenderer& text, const Scene& scene, float interpolation);

private:
    SpeechLayout layout_;
};
