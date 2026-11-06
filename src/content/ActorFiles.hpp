#pragma once

#include "game/Animation.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

struct ActorLoadResult {
    std::optional<AnimationSet> animations;
    std::string error;
};

// assets/actors/<id>.actor: an actor's clips, shared by every room, in the same fields as a
// room's sprites ("idle.file = player_idle.png", "left.fps = 8"). Rooms inherit them and may
// override single fields with player.<clip>.<field>.
namespace ActorFiles {
[[nodiscard]] ActorLoadResult parse(std::string_view content);
[[nodiscard]] ActorLoadResult load(const std::filesystem::path& path);
} // namespace ActorFiles
