#include <SDL3/SDL_main.h> // IWYU pragma: keep -- required by SDL on some platforms

#include "core/Application.hpp"
#include <cstdio>
#include <string_view>

int main(int argc, char* argv[]) {
    std::filesystem::path roomDirectory;
    if (argc == 3 && std::string_view(argv[1]) == "--rooms") roomDirectory = argv[2];
    else if (argc != 1) {
        std::fprintf(stderr, "Usage: pointclick [--rooms path/to/rooms]\n");
        return 1;
    }
    Application application(roomDirectory);
    return application.run();
}
