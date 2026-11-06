#include "game/ScriptRunner.hpp"

void ScriptRunner::start(const std::span<const InteractionEffect> steps, ScriptHost& host) {
    frames_.clear();
    push(steps, host);
}

void ScriptRunner::push(const std::span<const InteractionEffect> steps, ScriptHost& host,
                        const bool waitLast) {
    Frame frame;
    frame.steps.assign(steps.begin(), steps.end());
    frame.waitLast = waitLast;
    frames_.push_back(std::move(frame));
    runUntilBlocked(host);
}

void ScriptRunner::tick(const float deltaSeconds, ScriptHost& host) {
    if (frames_.empty()) {
        return;
    }
    frames_.back().step.elapsed += deltaSeconds;
    runUntilBlocked(host);
}

void ScriptRunner::cancel() noexcept {
    frames_.clear();
}

void ScriptRunner::runUntilBlocked(ScriptHost& host) {
    while (!frames_.empty()) {
        // Actions never push frames (see the class comment), so this reference stays valid.
        Frame& top = frames_.back();
        if (top.waiting) {
            const InteractionEffect& current = top.steps[top.next - 1];
            ActionContext context{host, top.step};
            if (current.effect->update(current.args, context) == ActionStatus::running) {
                return;
            }
            top.waiting = false;
        }
        while (top.next < top.steps.size()) {
            const InteractionEffect& current = top.steps[top.next++];
            if (!current.effect) {
                continue;
            }
            top.step = {};
            ActionContext context{host, top.step};
            const ActionStatus status = current.effect->start(current.args, context);
            const bool last = top.next >= top.steps.size();
            if (status == ActionStatus::running && current.effect->update
                && (!last || top.waitLast)) {
                top.waiting = true;
                return;
            }
        }
        frames_.pop_back();
    }
}
