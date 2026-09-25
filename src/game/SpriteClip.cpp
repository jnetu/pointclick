#include "game/SpriteClip.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>

std::string SpriteClip::validationError() const {
    if (!file.empty()) {
        const std::filesystem::path path(file);
        if (path.is_absolute() || path.extension() != ".png") return "Use um caminho relativo .png";
        for (const auto& part : path) {
            if (part == ".." || part == ".") return "Caminho de sprite invalido";
        }
    }
    if (frames < 1 || frames > 512 || columns < 0 || columns > 512
        || firstFrame < 0 || firstFrame > 4096) return "Contagem de quadros invalida";
    if (frameWidth < 0 || frameHeight < 0 || frameWidth > 8192 || frameHeight > 8192
        || (frameWidth == 0) != (frameHeight == 0)
        || (frames > 1 && frameWidth == 0)) return "Tamanho dos quadros invalido";
    if ((frameWidth == 0 && (firstFrame != 0 || columns != 0 || marginX != 0
                             || marginY != 0 || spacingX != 0 || spacingY != 0))
        || marginX < 0 || marginY < 0 || spacingX < 0 || spacingY < 0
        || marginX > 8192 || marginY > 8192 || spacingX > 8192 || spacingY > 8192) {
        return "Margem ou espacamento invalido";
    }
    if (!std::isfinite(fps) || fps < 0 || fps > 120
        || !std::isfinite(displayWidth) || displayWidth < 0 || displayWidth > 2048
        || !std::isfinite(displayHeight) || displayHeight < 0 || displayHeight > 2048
        || !std::isfinite(offsetX) || std::abs(offsetX) > 2048
        || !std::isfinite(offsetY) || std::abs(offsetY) > 2048) {
        return "FPS, tamanho na tela ou deslocamento invalido";
    }
    return {};
}

int SpriteClip::frameAt(const float elapsedSeconds) const {
    if (frames <= 1 || fps <= 0 || !std::isfinite(elapsedSeconds)) return 0;
    const double advance = std::max(0.0, static_cast<double>(elapsedSeconds)) * fps;
    if (!loop && advance >= frames) return frames - 1;
    return static_cast<int>(std::fmod(advance, static_cast<double>(frames)));
}

SDL_FRect SpriteClip::targetRect(const SDL_FPoint feet, const float baseWidth,
                                  const float baseHeight, const float scale) const {
    const float width = (displayWidth > 0 ? displayWidth : baseWidth) * scale;
    const float height = (displayHeight > 0 ? displayHeight : baseHeight) * scale;
    return {feet.x + offsetX * scale - width * 0.5F,
            feet.y + offsetY * scale - height, width, height};
}
