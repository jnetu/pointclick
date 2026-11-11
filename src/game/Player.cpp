#include "game/Player.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace {
// Locomotion poses follow Facing's order, one step after idle.
static_assert(static_cast<int>(PlayerPose::left) - 1 == static_cast<int>(Facing::left)
              && static_cast<int>(PlayerPose::right) - 1 == static_cast<int>(Facing::right)
              && static_cast<int>(PlayerPose::up) - 1 == static_cast<int>(Facing::up)
              && static_cast<int>(PlayerPose::down) - 1 == static_cast<int>(Facing::down));

Facing facingOf(const PlayerPose pose) { return static_cast<Facing>(static_cast<int>(pose) - 1); }
} // namespace

Player::Player(const SDL_FPoint start, const Facing facing)
        : previousFeet_(start), feet_(start), destination_(start), facing_(facing) {}

void Player::setPath(std::vector<SDL_FPoint> path) {
    if (path.empty()) {
        path_.clear();
        nextWaypoint_ = 0;
        destination_ = feet_;
        return;
    }
    destination_ = path.back();
    path_ = std::move(path);
    nextWaypoint_ = 0;
}

void Player::tick(const float deltaSeconds, const float speed) {
    previousFeet_ = feet_;
    float travel = std::max(speed, 0.0F) * deltaSeconds;
    while (travel > 0.0F && nextWaypoint_ < path_.size()) {
        const SDL_FPoint target = path_[nextWaypoint_];
        const float dx = target.x - feet_.x;
        const float dy = target.y - feet_.y;
        const float distance = std::hypot(dx, dy);
        if (distance <= travel) {
            feet_ = target;
            travel -= distance;
            ++nextWaypoint_;
        } else {
            feet_.x += dx * travel / distance;
            feet_.y += dy * travel / distance;
            travel = 0.0F;
        }
    }
    PlayerPose nextPose = PlayerPose::idle;
    if (moving()) {
        const float dx = feet_.x - previousFeet_.x;
        const float dy = feet_.y - previousFeet_.y;
        if (std::abs(dx) >= std::abs(dy)) {
            nextPose = dx < 0 ? PlayerPose::left : PlayerPose::right;
        } else {
            nextPose = dy < 0 ? PlayerPose::up : PlayerPose::down;
        }
        facing_ = facingOf(nextPose);
    }
    pose_ = nextPose;
    const bool changed = animation_.play(
            playerLocomotionAnimations[static_cast<std::size_t>(pose_)]);
    if (!changed) animation_.tick(deltaSeconds);
}

SDL_FPoint Player::feet() const { return feet_; }

SDL_FPoint Player::interpolatedFeet(const float alpha) const {
    const float t = std::clamp(alpha, 0.0F, 1.0F);
    return {std::lerp(previousFeet_.x, feet_.x, t),
            std::lerp(previousFeet_.y, feet_.y, t)};
}

SDL_FPoint Player::destination() const { return destination_; }

std::size_t Player::remainingWaypoints() const { return path_.size() - nextWaypoint_; }

std::span<const SDL_FPoint> Player::remainingPath() const {
    if (nextWaypoint_ >= path_.size()) {
        return {};
    }
    return {path_.data() + nextWaypoint_, path_.size() - nextWaypoint_};
}

bool Player::moving() const { return nextWaypoint_ < path_.size(); }

PlayerPose Player::pose() const { return pose_; }

Facing Player::facing() const { return facing_; }

void Player::face(const Facing facing) { facing_ = facing; }

std::string_view Player::animationName() const { return animation_.name(); }

float Player::animationTime() const { return animation_.elapsed(); }

float Player::interpolatedAnimationTime(const float alpha) const {
    return animation_.interpolatedElapsed(alpha);
}

std::string_view playerClipName(const Player& player, const AnimationSet& animations) {
    if (player.pose() == PlayerPose::idle) {
        const std::string_view standing =
                playerIdleAnimations[static_cast<std::size_t>(player.facing())];
        if (findAnimation(animations, standing)) return standing;
    }
    return player.animationName();
}
