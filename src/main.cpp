#include <SDL3/SDL_main.h> // IWYU pragma: keep -- required by SDL on some platforms

#include "core/Application.hpp"

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    Application application;
    return application.run();
}
