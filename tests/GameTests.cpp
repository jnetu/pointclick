#include "content/RoomPresets.hpp"
#include "game/Game.hpp"
#include "game/Navigation.hpp"

#include <cassert>
#include <cmath>

namespace {
bool near(const float a, const float b) { return std::abs(a - b) < 0.01F; }

void testMovementAndRetargeting() {
    Game game(RoomPresets::firstRoom());
    assert(near(game.player().feet().x, 640.0F));
    assert(near(game.player().feet().y, 360.0F));

    game.onClick({900.0F, 360.0F});
    game.tick(0.5F);
    assert(near(game.player().feet().x, 640.0F + game.room().speedAt(360.0F) * 0.5F));
    game.onClick({750.0F, 500.0F});
    for (int i = 0; i < 20; ++i) {
        game.tick(0.05F);
    }
    assert(near(game.player().feet().x, 750.0F));
    assert(near(game.player().feet().y, 500.0F));
    assert(!game.player().moving());

    game.onClick({-10.0F, 200.0F});
    assert(!game.player().moving());
    game.onClick({2.0F, 2.0F});
    assert(near(game.player().destination().x, 750.0F));
    assert(near(game.player().destination().y, 500.0F));
    game.onClick({2.0F, game.room().wallBottomY + 1.0F});
    const float expectedY = game.room().walkableTopY;
    assert(near(game.player().destination().y, expectedY));
    assert(near(game.player().destination().x,
                game.room().playerBaseSize.x * game.room().scaleAt(expectedY) * 0.5F));
}

void testRoomPerspective() {
    Room room = RoomPresets::firstRoom();
    assert(near(room.scaleAt(room.farDepthY), room.farScale));
    assert(near(room.scaleAt(room.nearDepthY), room.nearScale));
    assert(room.scaleAt(360.0F) < room.scaleAt(600.0F));
    assert(near(room.speedAt(room.farDepthY), room.playerFarSpeed));
    assert(near(room.speedAt(room.nearDepthY), room.playerNearSpeed));
    assert(room.speedAt(360.0F) < room.speedAt(600.0F));
    assert(room.playerBounds({640.0F, 360.0F}).w
           < room.playerBounds({640.0F, 600.0F}).w);

    room.wallBottomY = 350.0F;
    room.walkableTopY = 370.0F;
    room.farDepthY = 370.0F;
    room.farScale = 0.4F;
    room.nearScale = 2.0F;
    room.playerFarSpeed = 100.0F;
    room.playerNearSpeed = 100.0F;
    room.playerStart = {640.0F, 400.0F};
    Game custom(room);
    custom.onClick({800.0F, 320.0F});
    assert(!custom.player().moving());
    custom.onClick({800.0F, 400.0F});
    custom.tick(0.5F);
    assert(custom.player().moving());
    assert(near(std::hypot(custom.player().feet().x - room.playerStart.x,
                           custom.player().feet().y - room.playerStart.y), 50.0F));

    Room nextRoom = RoomPresets::firstRoom();
    nextRoom.playerStart = {300.0F, 620.0F};
    custom.loadRoom(nextRoom);
    assert(near(custom.player().feet().x, 300.0F));
    assert(near(custom.player().feet().y, 620.0F));
    assert(!custom.player().moving());
}

void testPerspectiveMovementSpeed() {
    Room room = RoomPresets::firstRoom();
    room.scenery.clear();
    room.playerStart = {500.0F, 320.0F};
    Game far(room);
    far.onClick({1000.0F, 320.0F});
    far.tick(0.5F);
    const float farDistance = far.player().feet().x - 500.0F;

    room.playerStart = {500.0F, 650.0F};
    Game nearGame(room);
    nearGame.onClick({1000.0F, 650.0F});
    nearGame.tick(0.5F);
    const float nearDistance = nearGame.player().feet().x - 500.0F;
    assert(nearDistance > farDistance);
    assert(near(farDistance, room.speedAt(320.0F) * 0.5F));
    assert(near(nearDistance, room.speedAt(650.0F) * 0.5F));
}

void testObstacleRoute() {
    Navigation corner;
    corner.setBlocked(1, 1, true);
    const auto aroundCorner = corner.findPath({31, 33}, {33, 31});
    assert(aroundCorner.size() > 1);
    assert(corner.findPath({10, 10}, {10, 720}).empty());
    Navigation navigation;
    for (int row = 8; row <= 13; ++row) {
        navigation.setBlocked(19, row, true);
    }
    const SDL_FPoint start{500.0F, 360.0F};
    const SDL_FPoint goal{750.0F, 360.0F};
    const auto route = navigation.findPath(start, goal);
    assert(route.size() > 1);
    assert(near(route.back().x, goal.x) && near(route.back().y, goal.y));

    SDL_FPoint from = start;
    for (const SDL_FPoint to : route) {
        const float length = std::hypot(to.x - from.x, to.y - from.y);
        const int steps = static_cast<int>(std::ceil(length));
        for (int i = 0; i <= steps; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            const int column = static_cast<int>((from.x + (to.x - from.x) * t) / Navigation::cellSize);
            const int row = static_cast<int>((from.y + (to.y - from.y) * t) / Navigation::cellSize);
            assert(column != 19 || row < 8 || row > 13);
        }
        from = to;
    }
    navigation.setBlocked(23, 11, true);
    assert(navigation.findPath(start, goal).empty());

    Navigation floorOnly;
    floorOnly.setWalkableTop(300.0F);
    for (int row = 9; row <= 14; ++row) {
        floorOnly.setBlocked(19, row, true);
    }
    const auto floorRoute = floorOnly.findPath(start, goal);
    assert(!floorRoute.empty());
    for (const SDL_FPoint waypoint : floorRoute) {
        assert(waypoint.y >= 300.0F);
    }
}

void testSolidObjectFootprint() {
    Room room = RoomPresets::firstRoom();
    assert(room.scenery[0].type == SceneObjectType::decoration);
    assert(room.scenery[1].type == SceneObjectType::solid);
    const SceneObject& red = room.scenery[1];
    const SDL_FRect footprint = red.worldCollisionFootprint();
    assert(footprint.w < red.bounds.w && footprint.h < red.bounds.h);

    Game game(room);
    game.onClick({860.0F, 500.0F}); // The upper visual area is passable.
    assert(near(game.player().destination().x, 860.0F));
    assert(near(game.player().destination().y, 500.0F));
    game.onClick({860.0F, 575.0F}); // The solid base snaps to reachable ground.
    const SDL_FPoint snapped = game.player().destination();
    assert(!SDL_PointInRectFloat(&snapped, &footprint));

    room.playerStart = {730.0F, 575.0F};
    Game detour(room);
    detour.onClick({1050.0F, 575.0F});
    assert(detour.player().remainingWaypoints() > 1);
    for (int i = 0; i < 100; ++i) {
        detour.tick(0.05F);
        const SDL_FPoint feet = detour.player().feet();
        assert(!SDL_PointInRectFloat(&feet, &footprint));
    }
    assert(!detour.player().moving());
    assert(near(detour.player().feet().x, 1050.0F));
    assert(near(detour.player().feet().y, 575.0F));
}

void testStandardRoomCameraFixed() {
    Game game(RoomPresets::firstRoom());
    assert(game.navigation().columns() == 40);
    assert(game.navigation().rows() == 23);
    assert(near(game.camera().position().x, 0.0F));
    assert(near(game.camera().position().y, 0.0F));

    const SDL_FPoint targets[]{{80.0F, 340.0F}, {1220.0F, 680.0F}, {640.0F, 500.0F}};
    for (const SDL_FPoint target : targets) {
        game.onClick(target);
        for (int i = 0; i < 60; ++i) {
            game.tick(0.05F);
            for (const float alpha : {0.0F, 0.5F, 1.0F}) {
                const SDL_FPoint cam = game.camera().interpolatedPosition(alpha);
                assert(near(cam.x, 0.0F) && near(cam.y, 0.0F));
            }
        }
    }
}

void testPanoramicRoomCameraAndNavigation() {
    Room room = RoomPresets::firstRoom();
    room.id = "panorama";
    room.width = 2560.0F;
    room.height = 720.0F;
    room.vanishingPointX = 1280.0F;
    room.playerStart = {200.0F, 560.0F};
    room.scenery = {
        {{1750.0F, 450.0F, 120.0F, 130.0F}, {220, 74, 68, 255}, SceneObjectType::solid,
         {10.0F, 95.0F, 100.0F, 35.0F}, "east_pillar", {}, "idle"},
    };
    const SDL_FRect eastFootprint = room.scenery[0].worldCollisionFootprint();

    Game game(room);
    assert(game.navigation().columns() == 80);
    assert(game.navigation().rows() == 23);
    // Spawn near the left border clamps the initial camera to x = 0.
    assert(near(game.camera().position().x, 0.0F));
    assert(near(game.camera().position().y, 0.0F));

    game.onClick({2350.0F, 560.0F});
    assert(game.player().moving());
    assert(game.player().remainingWaypoints() > 1);

    bool sawSmoothLag = false;
    bool crossedOriginalWidth = false;
    float previousCameraX = game.camera().position().x;
    for (int i = 0; i < 220; ++i) {
        const float beforeTickCamX = game.camera().position().x;
        game.tick(0.05F);
        const SDL_FPoint feet = game.player().feet();
        const SDL_FPoint cam = game.camera().position();
        const SDL_FPoint mid = game.camera().interpolatedPosition(0.5F);
        assert(!SDL_PointInRectFloat(&feet, &eastFootprint));
        assert(cam.x >= 0.0F && cam.x <= room.width - World::width);
        assert(near(cam.y, 0.0F));
        assert(cam.x + 0.001F >= previousCameraX);
        assert(mid.x >= beforeTickCamX - 0.001F && mid.x <= cam.x + 0.001F);
        if (feet.x > 900.0F && feet.x < 1600.0F && game.player().moving()) {
            const float idealTargetX = game.camera().targetFor(feet).x;
            if (cam.x > 0.0F && cam.x + 1.0F < idealTargetX) {
                sawSmoothLag = true;
            }
        }
        if (feet.x > 1280.0F) {
            crossedOriginalWidth = true;
        }
        previousCameraX = cam.x;
    }
    assert(crossedOriginalWidth);
    assert(sawSmoothLag);
    assert(!game.player().moving());
    assert(near(game.player().feet().x, 2350.0F));
    // Near the right border (2350 - 640 = 1710 > 1280), the camera clamps at 1280.
    assert(near(game.camera().position().x, 1280.0F));

    // Clicking inside the solid footprint at x > 1280 snaps to nearest walkable ground.
    game.onClick({1800.0F, 565.0F});
    const SDL_FPoint snapped = game.player().destination();
    assert(!SDL_PointInRectFloat(&snapped, &eastFootprint));

    // Teleporting snaps the camera immediately with zero lag.
    std::string error;
    assert(game.teleportPlayer({1400.0F, 520.0F}, error));
    assert(near(game.camera().position().x, 760.0F));
    assert(near(game.camera().interpolatedPosition(0.0F).x, 760.0F));

    // Live editor tweaks (resetPlayer == false) keep continuity and clamp when shrinking.
    Room faster = game.room();
    faster.playerFarSpeed = 160.0F;
    assert(game.applyRoom(faster, false, error));
    assert(near(game.camera().position().x, 760.0F));

    Room narrower = game.room();
    narrower.scenery.clear();
    narrower.width = 1800.0F;
    assert(game.applyRoom(narrower, false, error));
    assert(near(game.camera().position().x, 520.0F));

    // Loading a room strong-snaps both X and Y camera axes to the spawn target.
    Room tallAndWide = RoomPresets::firstRoom();
    tallAndWide.scenery.clear();
    tallAndWide.width = 2560.0F;
    tallAndWide.height = 1440.0F;
    tallAndWide.nearDepthY = 1440.0F;
    tallAndWide.playerStart = {1900.0F, 1000.0F};
    game.loadRoom(tallAndWide);
    assert(near(game.camera().position().x, 1260.0F));
    assert(near(game.camera().position().y, 640.0F));
}
}

int main() {
    testMovementAndRetargeting();
    testRoomPerspective();
    testPerspectiveMovementSpeed();
    testObstacleRoute();
    testSolidObjectFootprint();
    testStandardRoomCameraFixed();
    testPanoramicRoomCameraAndNavigation();
}
