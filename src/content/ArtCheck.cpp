#include "content/ArtCheck.hpp"

#include "content/TextFormat.hpp"

#include <SDL3/SDL_iostream.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <set>

namespace {
using FileHandle = std::unique_ptr<SDL_IOStream, decltype(&SDL_CloseIO)>;

FileHandle openForReading(const std::filesystem::path& path) {
    return {SDL_IOFromFile(TextFormat::pathToUtf8(path).c_str(), "rb"), SDL_CloseIO};
}

// SDL_ReadIO may return less than asked without being at the end.
bool readExactly(SDL_IOStream* file, const std::span<std::uint8_t> bytes) {
    std::size_t done = 0;
    while (done < bytes.size()) {
        const std::size_t count = SDL_ReadIO(file, bytes.data() + done, bytes.size() - done);
        if (count == 0) return false;
        done += count;
    }
    return true;
}

int bigEndian31(const std::uint8_t* bytes) {
    const std::uint32_t value = (std::uint32_t{bytes[0]} << 24) | (std::uint32_t{bytes[1]} << 16)
                                | (std::uint32_t{bytes[2]} << 8) | std::uint32_t{bytes[3]};
    // PNG dimensions are 1..2^31-1.
    return value > 0x7FFFFFFFU ? 0 : static_cast<int>(value);
}
} // namespace

ArtCheck::PngHeader ArtCheck::readPngHeader(const std::filesystem::path& path) {
    const FileHandle file = openForReading(path);
    if (!file) return {PngStatus::missing, {}};
    // 8 signature + 4 length + 4 "IHDR" + 4 width + 4 height.
    std::array<std::uint8_t, 24> header{};
    constexpr std::array<std::uint8_t, 16> expected{0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n',
                                                    0,    0,   0,   13,  'I',  'H',  'D',  'R'};
    if (!readExactly(file.get(), header)
        || !std::equal(expected.begin(), expected.end(), header.begin())) {
        return {PngStatus::unreadable, {}};
    }
    const ImageSize size{bigEndian31(&header[16]), bigEndian31(&header[20])};
    if (size.width <= 0 || size.height <= 0) return {PngStatus::unreadable, {}};
    return {PngStatus::ok, size};
}

std::vector<ArtWarning> ArtCheck::warnings(const Room& room,
                                           const std::filesystem::path& spritesDirectory,
                                           const std::filesystem::path& fontsDirectory,
                                           const std::span<const std::string_view> fontFiles) {
    std::vector<ArtWarning> found;
    std::map<std::string, PngHeader, std::less<>> headers; // one open per file per call
    const auto checkClip = [&](const std::string& owner, const std::string& name,
                               const SpriteClip& clip) {
        if (clip.file.empty()) {
            // Valid for the room, but SpriteLibrary::draw has nothing to load for it.
            found.push_back({"sprites", "Clip " + owner + "." + name + " sem arquivo PNG"});
            return;
        }
        auto known = headers.find(clip.file);
        const bool first = known == headers.end();
        if (first) {
            const std::u8string relative(clip.file.begin(), clip.file.end());
            known = headers.emplace(clip.file, readPngHeader(spritesDirectory / relative)).first;
        }
        const PngHeader& header = known->second;
        const std::string file = "sprites/" + clip.file;
        if (header.status != PngStatus::ok) {
            if (first) {
                found.push_back({file, header.status == PngStatus::missing ? "PNG ausente"
                                                                           : "PNG ilegivel"});
            }
            return;
        }
        if (!clipFitsImage(clip, header.size.width, header.size.height)) {
            found.push_back({file, "Corte de " + owner + "." + name + " nao cabe na imagem "
                                           + std::to_string(header.size.width) + "x"
                                           + std::to_string(header.size.height)});
        }
    };
    for (const auto& [name, clip] : room.playerAnimations) checkClip("player", name, clip);
    for (const SceneObject& object : room.scenery) {
        for (const auto& [name, clip] : object.animations) checkClip(object.id, name, clip);
    }
    std::set<std::string_view> fonts;
    for (const std::string_view font : fontFiles) {
        if (!fonts.insert(font).second) continue;
        const std::u8string relative(font.begin(), font.end());
        if (!openForReading(fontsDirectory / relative)) {
            found.push_back({"fonts/" + std::string(font), "Fonte ausente"});
        }
    }
    return found;
}
