#pragma once

#include <span>
#include <string_view>

struct Room;

// Shared by the file reader, validation and the in-game editor.
struct RoomParameter {
    std::string_view key;
    std::string_view description;
    float minimum;
    float maximum;
    float step;
    float (*read)(const Room&);
    void (*write)(Room&, float);
};

[[nodiscard]] std::span<const RoomParameter> roomParameters();
[[nodiscard]] const RoomParameter* findRoomParameter(std::string_view key);
