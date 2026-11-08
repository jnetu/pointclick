#pragma once

#include "game/ActionCatalog.hpp"
#include "game/Interaction.hpp"

#include <cstddef>
#include <span>
#include <vector>

// Runs catalog action sequences over ticks: instant steps run back to back, a running step holds
// its sequence (cutscene) until done. Sequences stack via push(); only the top one advances.
// Actions never push themselves: the host pushes after the action returns (no re-entry).
class ScriptRunner {
public:
    // Replaces every running sequence. Steps are copied, so reloading rules mid-script is safe.
    void start(std::span<const InteractionEffect> steps, ScriptHost& host);
    // Runs steps on top of the current sequences. With waitLast, a blocking last step holds the
    // sequence too (a reply that must be read before the next dialogue node opens).
    void push(std::span<const InteractionEffect> steps, ScriptHost& host, bool waitLast = false);
    void tick(float deltaSeconds, ScriptHost& host);
    void cancel() noexcept;

    // True while a blocking step holds the queue. Without waitLast a trailing say or walk_to
    // leaves the player in control.
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
