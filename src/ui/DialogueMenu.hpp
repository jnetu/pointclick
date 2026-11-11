#pragma once

#include "input/InputRouter.hpp"
#include "ui/TextLayout.hpp"

#include <SDL3/SDL_rect.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class Game;

// Dialogue choices UI. Acts only while Game::awaitingDialogueChoice(): then it is modal and turns
// presses into Game::chooseDialogueOption; the rest of the cutscene falls through to Game.
// Layout lives here so the hit-test and DialogueMenuRenderer share it. Rows grow with the real
// text: each "N. option" is wrapped by words (FontRole::choice) with the injected TextMetrics, so
// ui/ never includes graphics/. The layout is cached and rebuilt only when the options or the
// metrics change.
class DialogueMenu final : public ViewportUiConsumer {
public:
    static constexpr float kMargin = 24.0F;
    static constexpr float kPadding = 10.0F;
    static constexpr float kMinRowHeight = 40.0F;
    static constexpr float kRowPaddingY = 6.0F;
    static constexpr float kTextInsetX = 12.0F;

    struct Row {
        std::string label; // "1. text", what the lines index into
        std::vector<TextLine> lines;
        SDL_FRect rect{};
    };

    DialogueMenu(Game& game, const TextMetrics& metrics) : game_(game), metrics_(metrics) {}

    // Layout of the options offered now (no rows when the menu is closed).
    [[nodiscard]] SDL_FRect panelRect() const;
    [[nodiscard]] std::size_t rowCount() const;
    [[nodiscard]] const Row& row(std::size_t index) const;
    [[nodiscard]] SDL_FRect optionRect(std::size_t index) const;
    [[nodiscard]] std::optional<std::size_t> optionAt(SDL_FPoint viewportPoint) const;
    // Top-left of one wrapped line of a row, the block centered vertically in the row.
    [[nodiscard]] SDL_FPoint lineOrigin(std::size_t rowIndex, std::size_t lineIndex) const;

    // Option under the pointer or picked with up/down; always < the option count when open.
    [[nodiscard]] std::size_t highlighted() const;

    bool onPointerMove(SDL_FPoint viewportPoint, bool insideViewport) override;
    bool onPrimaryPress(SDL_FPoint viewportPoint) override;
    bool onNavigation(UiNavAction action) override;

private:
    [[nodiscard]] bool open() const;
    bool choose(std::size_t index);
    void refreshLayout() const;

    Game& game_;
    const TextMetrics& metrics_;
    // Cache key: the option texts and the metrics revision the rows were built for.
    mutable std::vector<std::string> layoutTexts_;
    mutable std::uint32_t layoutRevision_ = 0;
    mutable bool layoutBuilt_ = false;
    mutable std::vector<Row> rows_;
    mutable SDL_FRect panel_{};
    std::size_t highlighted_ = 0;
    // Up/down moved the highlight and the pointer has not moved since: a press away from the
    // options (gamepad South at the virtual cursor) confirms the highlighted one.
    bool navigationFocus_ = false;
    std::optional<SDL_FPoint> lastPointer_;
};
