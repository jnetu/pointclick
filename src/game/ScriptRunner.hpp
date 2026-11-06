#pragma once

#include "game/ActionCatalog.hpp"
#include "game/Interaction.hpp"

#include <cstddef>
#include <span>
#include <vector>

// Runs sequences of catalog actions over several ticks. Instant steps run back to back in the
// same call; a step that returns running holds its sequence until its update reports done.
// While a step holds the queue the scene is in cutscene: Game ignores world clicks and only
// lets a click skip the current line. Rules, room enter events and dialogue nodes all feed the
// same runner with their step lists.
//
// Sequences stack: push() runs a nested sequence on top of the current one (a dialogue node
// opened by start_dialogue). Only the top sequence advances; when it ends, the one below
// re-polls its waiting step at once. Actions never push themselves: the host defers the
// request and pushes after the action returns, so a running step is never re-entered.
class ScriptRunner {
public:
    // Replaces every running sequence. Steps are copied, so reloading rules mid-script is safe.
    void start(std::span<const InteractionEffect> steps, ScriptHost& host);
    // Runs steps on top of the current sequences. With waitLast, a blocking last step holds the
    // sequence too (a reply that must be read before the next dialogue node opens).
    void push(std::span<const InteractionEffect> steps, ScriptHost& host, bool waitLast = false);
    void tick(float deltaSeconds, ScriptHost& host);
    void cancel() noexcept;

    // True while a blocking step holds the queue. Without waitLast the last step is never
    // waited on: nothing after it depends on it, so a trailing say or walk_to leaves the player
    // in control.
    [[nodiscard]] bool busy() const noexcept { return !frames_.empty(); }
    // Number of stacked sequences still holding the queue.
    [[nodiscard]] std::size_t depth() const noexcept { return frames_.size(); }

private:
    struct Frame {
        std::vector<InteractionEffect> steps;
        std::size_t next = 0;
        bool waiting = false;
        bool waitLast = false;
        StepState step;
    };

    void runUntilBlocked(ScriptHost& host);

    std::vector<Frame> frames_;
};
