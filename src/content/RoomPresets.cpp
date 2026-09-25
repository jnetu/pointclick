#include "content/RoomPresets.hpp"

Room RoomPresets::defaults() {
    Room room;
    room.playerAnimations = {
        {"idle", SpriteClip{.file = "player_idle.png", .frameWidth = 32, .frameHeight = 48,
                              .frames = 8, .columns = 1, .fps = 6,
                              .displayWidth = 53.333333F}},
        {"left", SpriteClip{.file = "player_esquerda.png", .frameWidth = 32, .frameHeight = 48,
                              .frames = 7, .columns = 7, .spacingX = 1,
                              .displayWidth = 53.333333F}},
        {"right", SpriteClip{.file = "player_direita.png", .frameWidth = 32,
                               .frameHeight = 48, .frames = 7, .columns = 7,
                               .spacingX = 1, .displayWidth = 53.333333F}},
        {"up", SpriteClip{.file = "player_cima.png", .frameWidth = 32, .frameHeight = 48,
                            .frames = 7, .columns = 7, .spacingX = 1,
                            .displayWidth = 53.333333F}},
        {"down", SpriteClip{.file = "player_baixo.png", .frameWidth = 32,
                              .frameHeight = 48, .frames = 7, .columns = 7,
                              .spacingX = 1, .displayWidth = 53.333333F}},
    };
    return room;
}

// Built-in example for tests and embedding. The application loads first.room.
Room RoomPresets::firstRoom() {
    Room room = RoomPresets::defaults();
    room.scenery = {
        {{380, 345, 100, 135}, {89, 127, 157, 255}, SceneObjectType::decoration, {}, "blue_box", {}, "idle"},
        {{810, 465, 110, 125}, {220, 74, 68, 255}, SceneObjectType::solid,
         {14, 92, 82, 33}, "red_box", {}, "idle"},
    };
    return room;
}
