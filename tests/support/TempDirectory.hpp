#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

// A scratch directory per test case, removed when the case ends (also when a REQUIRE fails).
// Each case owns its folder, so a doctest filter can run any case alone.
class TempDirectory {
public:
    explicit TempDirectory(const std::string_view prefix) {
        static int counter = 0;
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path()
                / (std::string(prefix) + "-" + std::to_string(suffix) + "-"
                   + std::to_string(counter++));
        std::filesystem::create_directory(path_);
    }
    ~TempDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }
    TempDirectory(const TempDirectory&) = delete;
    TempDirectory& operator=(const TempDirectory&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};
