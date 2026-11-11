#pragma once

#include "game/Room.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Art the game would have to replace on screen: a warning, never an error. The room still loads
// and runs; a missing sprite draws the purple checkerboard and a missing font falls back to the
// debug font. This only tells the designer before the problem shows up in a frame.
struct ArtWarning {
    std::string file;    // "sprites/door.png", "fonts/lora/Lora-Regular.ttf"
    std::string message; // "PNG ausente", "Corte de door.open nao cabe na imagem 64x32"

    [[nodiscard]] std::string text() const { return file + ": " + message; }
    [[nodiscard]] bool operator==(const ArtWarning&) const = default;
};

namespace ArtCheck {
struct ImageSize {
    int width = 0;
    int height = 0;
};

enum class PngStatus : std::uint8_t { ok, missing, unreadable };
struct PngHeader {
    PngStatus status = PngStatus::missing;
    ImageSize size{};
};
// Reads the signature and the IHDR chunk only (24 bytes) through SDL_IOStream, so it works on
// assets inside an APK and never decodes the pixels.
[[nodiscard]] PngHeader readPngHeader(const std::filesystem::path& path);

// Every PNG the room's clips name (player and objects): missing or unreadable file (once per
// file), each cut that does not fit its image and each clip that names no file. Then every font of fontFiles that cannot be
// opened in fontsDirectory; the caller supplies that list (FontLibrary::files), so content/
// does not depend on graphics/. Each file is opened once per call.
[[nodiscard]] std::vector<ArtWarning> warnings(const Room& room,
                                               const std::filesystem::path& spritesDirectory,
                                               const std::filesystem::path& fontsDirectory = {},
                                               std::span<const std::string_view> fontFiles = {});
} // namespace ArtCheck
