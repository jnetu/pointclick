#pragma once

#include "game/SpriteClip.hpp"
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>

struct SDL_Renderer;
struct SDL_Texture;

// Renderer-owned PNG cache. Missing or invalid assets render as a checkerboard.
class SpriteLibrary {
public:
    SpriteLibrary() = default;
    ~SpriteLibrary();
    SpriteLibrary(const SpriteLibrary&) = delete;
    SpriteLibrary& operator=(const SpriteLibrary&) = delete;

    void initialize(SDL_Renderer* renderer);
    void setRoot(std::filesystem::path root);
    void clear();
    [[nodiscard]] bool draw(const SpriteClip& clip, SDL_FRect target,
                            float elapsedSeconds, bool flipHorizontally = false);

private:
    struct Image {
        SDL_Texture* texture = nullptr;
        int width = 0;
        int height = 0;
    };

    Image& imageFor(const std::string& file);
    void drawMissing(SDL_FRect target) const;

    SDL_Renderer* renderer_ = nullptr;
    std::filesystem::path root_;
    std::unordered_map<std::string, Image> images_;
    std::unordered_set<std::string> missingLogged_;
};
