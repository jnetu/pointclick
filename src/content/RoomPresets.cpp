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

RoomInteractions RoomPresets::firstRoomInteractions() {
    return RoomInteractions{{
        {
            .trigger = InteractionTrigger::interact("blue_box"),
            .conditions = {InteractionCondition::flagNotSet("found_brass_key")},
            .effects = {
                InteractionEffect::setFlag("found_brass_key"),
                InteractionEffect::giveItem("brass_key"),
                InteractionEffect::say("Encontrei uma chave de latao escondida na caixa azul!"),
            },
        },
        {
            .trigger = InteractionTrigger::interact("blue_box"),
            .conditions = {},
            .effects = {InteractionEffect::say("A caixa azul ja foi vasculhada.")},
        },
        {
            .trigger = InteractionTrigger::useItem("brass_key", "red_box"),
            .conditions = {InteractionCondition::flagNotSet("red_box_unlocked")},
            .effects = {
                InteractionEffect::takeItem("brass_key"),
                InteractionEffect::setFlag("red_box_unlocked"),
                InteractionEffect::setSolid("red_box", false),
                InteractionEffect::say("A chave destravou a caixa vermelha! O caminho esta livre."),
            },
        },
        {
            .trigger = InteractionTrigger::interact("red_box"),
            .conditions = {InteractionCondition::flagSet("red_box_unlocked")},
            .effects = {InteractionEffect::say("A trava foi aberta. A caixa nao bloqueia mais a passagem.")},
        },
        {
            .trigger = InteractionTrigger::interact("red_box"),
            .conditions = {InteractionCondition::hasItem("brass_key")},
            .effects = {
                InteractionEffect::takeItem("brass_key"),
                InteractionEffect::setFlag("red_box_unlocked"),
                InteractionEffect::setSolid("red_box", false),
                InteractionEffect::say("Usei a chave de latao e liberei a passagem da caixa vermelha!"),
            },
        },
        {
            .trigger = InteractionTrigger::interact("red_box"),
            .conditions = {},
            .effects = {InteractionEffect::say("Esta pesada e trancada no chao. Talvez haja uma chave por perto.")},
        },
    }};
}
