#include "game/Room.hpp"

// Built-in example for tests and embedding. The application loads first.room.
Room Room::firstRoom() {
    Room room;
    room.scenery = {
        {{380, 345, 100, 135}, {89, 127, 157, 255}, SceneObjectType::decoration, {}, "blue_box", {}},
        {{810, 465, 110, 125}, {220, 74, 68, 255}, SceneObjectType::solid,
         {14, 92, 82, 33}, "red_box", {}},
    };
    return room;
}
