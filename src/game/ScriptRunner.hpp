#pragma once

#include "game/ActionCatalog.hpp"
#include "game/Interaction.hpp"

#include <cstddef>
#include <span>
#include <vector>

// Runs a sequence of catalog actions over several ticks. Instant steps run back to back in the
// same call; a step that returns running holds the queue until its update reports done.
// While a step holds the queue the scene is in cutscene: Game ignores world clicks and only
// lets a click skip the current line. Rules, room enter events and dialogue nodes all feed the
// same runner with their step lists.
class ScriptRunner {
public:
    // Replaces any running sequence. Steps are copied, so reloading rules mid-script is safe.
    void start(std::span<const InteractionEffect> steps, ScriptHost& host);
    void tick(float deltaSeconds, ScriptHost& host);
    void cancel() noexcept;

    // True while a blocking step holds the queue. The last step is never waited on: nothing
    // after it depends on it, so a trailing say or walk_to leaves the player in control.
    [[nodiscard]] bool busy() const noexcept { return waiting_; }

private:
    void runUntilBlocked(ScriptHost& host);

    std::vector<InteractionEffect> steps_;
    std::size_t next_ = 0;
    bool waiting_ = false;
    StepState step_;
};
