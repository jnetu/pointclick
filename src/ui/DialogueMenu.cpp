#include "ui/DialogueMenu.hpp"

#include "game/Game.hpp"
#include "game/World.hpp"

SDL_FRect DialogueMenu::panelRect(const std::size_t optionCount) {
    const float height = kPadding * 2.0F + kRowHeight * static_cast<float>(optionCount);
    return {kMargin, static_cast<float>(World::height) - kMargin - height,
            static_cast<float>(World::width) - kMargin * 2.0F, height};
}

SDL_FRect DialogueMenu::optionRect(const std::size_t index, const std::size_t optionCount) {
    const SDL_FRect panel = panelRect(optionCount);
    return {panel.x + kPadding, panel.y + kPadding + kRowHeight * static_cast<float>(index),
            panel.w - kPadding * 2.0F, kRowHeight};
}

std::optional<std::size_t> DialogueMenu::optionAt(const SDL_FPoint viewportPoint,
                                                  const std::size_t optionCount) {
    for (std::size_t i = 0; i < optionCount; ++i) {
        const SDL_FRect row = optionRect(i, optionCount);
        if (SDL_PointInRectFloat(&viewportPoint, &row)) return i;
    }
    return std::nullopt;
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
    const std::size_t count = game_.dialogueOptions().size();
    if (const auto hovered = optionAt(viewportPoint, count)) {
        highlighted_ = *hovered;
        navigationFocus_ = false;
    }
    const SDL_FRect panel = panelRect(count);
    return insideViewport && SDL_PointInRectFloat(&viewportPoint, &panel);
}

bool DialogueMenu::onPrimaryPress(const SDL_FPoint viewportPoint) {
    if (!open()) return false;
    const std::size_t count = game_.dialogueOptions().size();
    if (const auto pressed = optionAt(viewportPoint, count)) {
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
