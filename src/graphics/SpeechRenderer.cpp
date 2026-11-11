#include "graphics/SpeechRenderer.hpp"

#include "game/Scene.hpp"
#include "graphics/EntityRenderer.hpp"
#include "graphics/TextRenderer.hpp"

namespace {
constexpr SDL_Color kLineColor{250, 236, 170, 255};
constexpr SDL_Color kSpeakerColor{168, 214, 255, 255};
} // namespace

void SpeechRenderer::draw(TextRenderer& text, const Scene& scene, const float interpolation) {
    const FloatingText& line = scene.floatingText();
    if (!line.isVisible()) return;
    (void)layout_.update(line.text(), line.speaker(), text);

    const SDL_FPoint camera = scene.camera().interpolatedPosition(interpolation);
    const SDL_FPoint feet = scene.player().interpolatedFeet(interpolation);
    const SDL_FRect body = EntityRenderer::playerRect(scene.room(), scene.player(), feet, camera);
    layout_.place({body.x + body.w * 0.5F, body.y});

    if (!layout_.speaker().empty()) {
        text.draw(FontRole::speaker, layout_.speaker(), layout_.speakerOrigin(), kSpeakerColor);
    }
    const auto& lines = layout_.lines();
    for (std::size_t i = 0; i < lines.size(); ++i) {
        text.draw(FontRole::dialogue, lines[i].in(layout_.text()), layout_.lineOrigin(i),
                  kLineColor);
    }
}
