#pragma once

#include "input/InputRouter.hpp"

#include <SDL3/SDL_rect.h>

#include <cstddef>
#include <optional>

class Game;

// Gameplay UI for dialogue choices, in the fixed 1280x720 viewport. It only acts while
// Game::awaitingDialogueChoice() (a moment inside the cutscene): then it is modal, consumes every
// press and navigation, and turns them into Game::chooseDialogueOption. The rest of the cutscene
// (lines being spoken, replies) falls through to Game, where a click skips the line.
// Layout lives here so the hit-test and graphics/DialogueMenuRenderer share it.
class DialogueMenu final : public ViewportUiConsumer {
public:
    static constexpr float kMargin = 24.0F;
    static constexpr float kPadding = 10.0F;
    static constexpr float kRowHeight = 40.0F;

    explicit DialogueMenu(Game& game) : game_(game) {}

    [[nodiscard]] static SDL_FRect panelRect(std::size_t optionCount);
    [[nodiscard]] static SDL_FRect optionRect(std::size_t index, std::size_t optionCount);
    [[nodiscard]] static std::optional<std::size_t> optionAt(SDL_FPoint viewportPoint,
                                                             std::size_t optionCount);

    // Option under the pointer or picked with up/down; always < the option count when open.
    [[nodiscard]] std::size_t highlighted() const;

    bool onPointerMove(SDL_FPoint viewportPoint, bool insideViewport) override;
    bool onPrimaryPress(SDL_FPoint viewportPoint) override;
    bool onNavigation(UiNavAction action) override;

private:
    [[nodiscard]] bool open() const;
    bool choose(std::size_t index);

    Game& game_;
    std::size_t highlighted_ = 0;
    // Up/down moved the highlight and the pointer has not moved since: a press away from the
    // options (gamepad South at the virtual cursor) confirms the highlighted one.
    bool navigationFocus_ = false;
    std::optional<SDL_FPoint> lastPointer_;
};
