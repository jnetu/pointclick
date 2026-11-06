#include "game/ScriptRunner.hpp"

void ScriptRunner::start(const std::span<const InteractionEffect> steps, ScriptHost& host) {
    steps_.assign(steps.begin(), steps.end());
    next_ = 0;
    runUntilBlocked(host);
}

void ScriptRunner::tick(const float deltaSeconds, ScriptHost& host) {
    if (!waiting_) {
        return;
    }
    step_.elapsed += deltaSeconds;
    const InteractionEffect& current = steps_[next_ - 1];
    ActionContext context{host, step_};
    if (current.effect->update(current.args, context) == ActionStatus::running) {
        return;
    }
    runUntilBlocked(host);
}

void ScriptRunner::cancel() noexcept {
    steps_.clear();
    next_ = 0;
    waiting_ = false;
}

void ScriptRunner::runUntilBlocked(ScriptHost& host) {
    waiting_ = false;
    while (next_ < steps_.size()) {
        const InteractionEffect& current = steps_[next_++];
        if (!current.effect) {
            continue;
        }
        step_ = {};
        ActionContext context{host, step_};
        const ActionStatus status = current.effect->start(current.args, context);
        if (status == ActionStatus::running && current.effect->update && next_ < steps_.size()) {
            waiting_ = true;
            return;
        }
    }
    steps_.clear();
    next_ = 0;
}
