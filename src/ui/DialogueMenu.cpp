#include "ui/DialogueMenu.hpp"

#include "game/Game.hpp"
#include "game/World.hpp"

#include <algorithm>
#include <string>

void DialogueMenu::refreshLayout() const {
    const auto options = game_.dialogueOptions();
    const std::size_t count = game_.awaitingDialogueChoice() ? options.size() : 0;
    bool same = layoutBuilt_ && layoutRevision_ == metrics_.revision()
                && layoutTexts_.size() == count;
    for (std::size_t i = 0; same && i < count; ++i) same = layoutTexts_[i] == options[i]->text;
    if (same) return;

    layoutBuilt_ = true;
    layoutRevision_ = metrics_.revision();
    layoutTexts_.resize(count);
    rows_.resize(count);
    const float lineHeight = metrics_.lineHeight(FontRole::choice);
    const float panelWidth = static_cast<float>(World::width) - kMargin * 2.0F;
    const float rowWidth = panelWidth - kPadding * 2.0F;
    const float textWidth = rowWidth - kTextInsetX * 2.0F;
    float height = kPadding * 2.0F;
    for (std::size_t i = 0; i < count; ++i) {
        layoutTexts_[i] = options[i]->text;
        Row& row = rows_[i];
        row.label = std::to_string(i + 1) + ". " + options[i]->text;
        TextLayout::wrap(row.label, textWidth, metrics_, FontRole::choice, row.lines);
        const float textHeight = lineHeight * static_cast<float>(row.lines.size());
        row.rect = {0.0F, 0.0F, rowWidth, std::max(kMinRowHeight, textHeight + kRowPaddingY * 2.0F)};
        height += row.rect.h;
    }
    // Anchored to the bottom of the viewport; a very long menu stops at the top margin.
    const float top = std::max(kMargin, static_cast<float>(World::height) - kMargin - height);
    panel_ = {kMargin, top, panelWidth, height};
    float y = top + kPadding;
    for (Row& row : rows_) {
        row.rect.x = kMargin + kPadding;
        row.rect.y = y;
        y += row.rect.h;
    }
}

SDL_FRect DialogueMenu::panelRect() const {
    refreshLayout();
    return panel_;
}

std::size_t DialogueMenu::rowCount() const {
    refreshLayout();
    return rows_.size();
}

const DialogueMenu::Row& DialogueMenu::row(const std::size_t index) const {
    refreshLayout();
    return rows_.at(index);
}

SDL_FRect DialogueMenu::optionRect(const std::size_t index) const {
    return row(index).rect;
}

std::optional<std::size_t> DialogueMenu::optionAt(const SDL_FPoint viewportPoint) const {
    refreshLayout();
    for (std::size_t i = 0; i < rows_.size(); ++i) {
        if (SDL_PointInRectFloat(&viewportPoint, &rows_[i].rect)) return i;
    }
    return std::nullopt;
}

SDL_FPoint DialogueMenu::lineOrigin(const std::size_t rowIndex, const std::size_t lineIndex) const {
    const Row& target = row(rowIndex);
    const float lineHeight = metrics_.lineHeight(FontRole::choice);
    const float textHeight = lineHeight * static_cast<float>(target.lines.size());
    return {target.rect.x + kTextInsetX,
            target.rect.y + (target.rect.h - textHeight) * 0.5F
                    + lineHeight * static_cast<float>(lineIndex)};
}

std::size_t DialogueMenu::highlighted() const {
    const std::size_t count = game_.dialogueOptions().size();
    return count == 0 ? 0 : (highlighted_ < count ? highlighted_ : count - 1);
}

bool DialogueMenu::open() const {
    return game_.awaitingDialogueChoice() && !game_.dialogueOptions().empty();
}

bool DialogueMenu::choose(const std::size_t index) {
    highlighted_ = 0;
    navigationFocus_ = false;
    return game_.chooseDialogueOption(index);
}

bool DialogueMenu::onPointerMove(const SDL_FPoint viewportPoint, const bool insideViewport) {
    const bool moved = !lastPointer_ || lastPointer_->x != viewportPoint.x
                       || lastPointer_->y != viewportPoint.y;
    lastPointer_ = viewportPoint;
    if (!open()) return false;
    if (moved) navigationFocus_ = false;
    if (const auto hovered = optionAt(viewportPoint)) {
        highlighted_ = *hovered;
        navigationFocus_ = false;
    }
    const SDL_FRect panel = panelRect();
    return insideViewport && SDL_PointInRectFloat(&viewportPoint, &panel);
}

bool DialogueMenu::onPrimaryPress(const SDL_FPoint viewportPoint) {
    if (!open()) return false;
    if (const auto pressed = optionAt(viewportPoint)) {
        (void)choose(*pressed);
    } else if (navigationFocus_) {
        (void)choose(highlighted());
    }
    return true; // modal while choosing: the world never sees this press
}

bool DialogueMenu::onNavigation(const UiNavAction action) {
    if (!open()) return false;
    const std::size_t count = game_.dialogueOptions().size();
    switch (action) {
    case UiNavAction::up:
        highlighted_ = (highlighted() + count - 1) % count;
        navigationFocus_ = true;
        break;
    case UiNavAction::down:
        highlighted_ = (highlighted() + 1) % count;
        navigationFocus_ = true;
        break;
    case UiNavAction::confirm:
        (void)choose(highlighted());
        break;
    case UiNavAction::left:
    case UiNavAction::right:
    case UiNavAction::cancel:
        break;
    }
    return true;
}
