#include "support/RoomPresets.hpp"

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

// Built-in example for tests. The application loads first.room from disk.
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
            .conditions = {InteractionCondition::make("flag_not_set", {"found_brass_key"})},
            .effects = {
                InteractionEffect::make("set_flag", {"found_brass_key"}),
                InteractionEffect::make("give_item", {"brass_key"}),
                InteractionEffect::make("say", {"Encontrei uma chave de latao escondida na caixa azul!"}),
            },
        },
        {
            .trigger = InteractionTrigger::interact("blue_box"),
            .conditions = {},
            .effects = {InteractionEffect::make("say", {"A caixa azul ja foi vasculhada."})},
        },
        {
            .trigger = InteractionTrigger::useItem("brass_key", "red_box"),
            .conditions = {InteractionCondition::make("flag_not_set", {"red_box_unlocked"})},
            .effects = {
                InteractionEffect::make("take_item", {"brass_key"}),
                InteractionEffect::make("set_flag", {"red_box_unlocked"}),
                InteractionEffect::make("set_solid", {"red_box", "0"}),
                InteractionEffect::make("say", {"A chave destravou a caixa vermelha! O caminho esta livre."}),
            },
        },
        {
            .trigger = InteractionTrigger::interact("red_box"),
            .conditions = {InteractionCondition::make("flag_set", {"red_box_unlocked"})},
            .effects = {InteractionEffect::make("say", {"A trava foi aberta. A caixa nao bloqueia mais a passagem."})},
        },
        {
            .trigger = InteractionTrigger::interact("red_box"),
            .conditions = {InteractionCondition::make("has_item", {"brass_key"})},
            .effects = {
                InteractionEffect::make("take_item", {"brass_key"}),
                InteractionEffect::make("set_flag", {"red_box_unlocked"}),
                InteractionEffect::make("set_solid", {"red_box", "0"}),
                InteractionEffect::make("say", {"Usei a chave de latao e liberei a passagem da caixa vermelha!"}),
            },
        },
        {
            .trigger = InteractionTrigger::interact("red_box"),
            .conditions = {},
            .effects = {InteractionEffect::make("say", {"Esta pesada e trancada no chao. Talvez haja uma chave por perto."})},
        },
    }};
}
