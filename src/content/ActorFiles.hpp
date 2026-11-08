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

// assets/actors/<id>.actor: clips shared by every room, in the .room sprite fields
// ("left.fps = 8"). A room may override single fields with player.<clip>.<field>.
namespace ActorFiles {
[[nodiscard]] ActorLoadResult parse(std::string_view content);
[[nodiscard]] ActorLoadResult load(const std::filesystem::path& path);
} // namespace ActorFiles
