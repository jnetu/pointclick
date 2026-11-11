#include <SDL3/SDL_main.h> // IWYU pragma: keep -- required by SDL on some platforms

#include "content/ContentCheck.hpp"
#include "core/Application.hpp"
#include "core/CommandLine.hpp"
#include "graphics/FontLibrary.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>

#include <cstdio>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
// --check: no window, no SDL subsystem; errors on stderr as "<arquivo>: Linha N: ...".
// Missing art is "aviso: <sala>: <arquivo>: ..." on stderr too: counted, never the exit code.
int runContentCheck(std::filesystem::path roomsDirectory) {
    if (roomsDirectory.empty()) {
        const char* base = SDL_GetBasePath();
        if (!base) {
            std::fprintf(stderr, "Nao foi possivel localizar os assets: %s\n", SDL_GetError());
            return 1;
        }
        const std::u8string basePath(base, base + SDL_strlen(base));
        roomsDirectory = std::filesystem::path(basePath) / "assets" / "rooms";
    }
    const auto fonts = FontLibrary::files();
    const ContentCheckReport report = checkContent(roomsDirectory, fonts);
    for (const ContentProblem& warning : report.warnings) {
        std::fprintf(stderr, "aviso: %s: %s\n", warning.file.c_str(), warning.message.c_str());
    }
    for (const ContentProblem& problem : report.problems) {
        std::fprintf(stderr, "%s: %s\n", problem.file.c_str(), problem.message.c_str());
    }
    std::printf("%zu sala(s) verificada(s), %zu erro(s), %zu aviso(s)\n", report.roomsChecked,
                report.problems.size(), report.warnings.size());
    return report.ok() ? 0 : 1;
}
} // namespace

int main(int argc, char* argv[]) {
    const std::vector<std::string_view> args(argv + (argc > 0 ? 1 : 0), argv + argc);
    CommandLineResult parsed = parseCommandLine(args);
    if (!parsed.options) {
        const std::string_view usage = commandLineUsage();
        std::fprintf(stderr, "%s\n%.*s", parsed.error.c_str(), static_cast<int>(usage.size()),
                     usage.data());
        return 2;
    }
    if (parsed.options->check) return runContentCheck(parsed.options->roomsDirectory);
    Application application(std::move(*parsed.options));
    return application.run();
}
