#include "game/ActionCatalog.hpp"

#include "game/Facing.hpp"
#include "game/Identifier.hpp"
#include "game/Room.hpp"
#include "game/SessionState.hpp"

#include <optional>

namespace {
using enum ArgType;

std::string validateObjectAnimation(const ActionArgs args, const Room* room) {
    if (!room) return {};
    const SceneObject* target = room->findObject(args[0]);
    if (target && !findAnimation(target->animations, args[1])) {
        return "Animacao desconhecida '" + args[1] + "' no objeto: " + args[0];
    }
    return {};
}

constexpr ActionStatus kDone = ActionStatus::done;
constexpr ActionStatus kRunning = ActionStatus::running;

constexpr float kMaxNumberArgument = 3600.0F;

const EffectDescriptor kEffects[]{
    // Blocks until the line expires or a click skips it.
    {"say", "Exibe a fala/observacao; espera o tempo de leitura ou um clique", {text}, nullptr,
     [](const ActionArgs args, ActionContext& context) {
         context.host.say(args[0]);
         return kRunning;
     },
     [](ActionArgs, ActionContext& context) { return context.host.speaking() ? kRunning : kDone; }},
    {"set_flag", "Ativa uma flag na sessao", {flagId}, nullptr,
     [](const ActionArgs args, ActionContext& context) {
         context.host.session().setFlag(args[0]);
         return kDone;
     },
     nullptr},
    {"clear_flag", "Limpa uma flag na sessao", {flagId}, nullptr,
     [](const ActionArgs args, ActionContext& context) {
         context.host.session().clearFlag(args[0]);
         return kDone;
     },
     nullptr},
    {"add_flag", "Soma <n> (pode ser negativo) ao contador da flag; ausente conta como 0",
     {flagId, integer}, nullptr,
     [](const ActionArgs args, ActionContext& context) {
         int delta = 0;
         (void)parseIntegerArgument(args[1], delta);
         (void)context.host.session().incrementFlag(args[0], delta);
         return kDone;
     },
     nullptr},
    {"give_item", "Adiciona um item ao inventario", {itemId}, nullptr,
     [](const ActionArgs args, ActionContext& context) {
         context.host.session().addItem(args[0]);
         return kDone;
     },
     nullptr},
    {"take_item", "Remove um item do inventario", {itemId}, nullptr,
     [](const ActionArgs args, ActionContext& context) {
         context.host.session().removeItem(args[0]);
         return kDone;
     },
     nullptr},
    {"hide_object", "Esconde um objeto da sala", {objectId}, nullptr,
     [](const ActionArgs args, ActionContext& context) {
         context.host.session().setObjectVisible(context.host.roomId(), args[0], false);
         context.host.invalidateNavigation();
         return kDone;
     },
     nullptr},
    {"show_object", "Revela um objeto da sala", {objectId}, nullptr,
     [](const ActionArgs args, ActionContext& context) {
         context.host.session().setObjectVisible(context.host.roomId(), args[0], true);
         context.host.invalidateNavigation();
         return kDone;
     },
     nullptr},
    {"set_solid", "Liga (1) ou desliga (0) a colisao e reconstroi a navegacao",
     {objectId, boolean}, nullptr,
     [](const ActionArgs args, ActionContext& context) {
         context.host.session().setObjectSolid(context.host.roomId(), args[0], args[1] == "1");
         context.host.invalidateNavigation();
         return kDone;
     },
     nullptr},
    {"play_animation", "Toca/reinicia um clip configurado no objeto", {objectId, animation},
     validateObjectAnimation,
     [](const ActionArgs args, ActionContext& context) {
         context.host.playAnimation(args[0], args[1]);
         return kDone;
     },
     nullptr},
    // Clips with loop = true never end, so waiting on them returns at once.
    {"wait_animation", "Espera o clip atual do objeto terminar", {objectId}, nullptr,
     [](const ActionArgs args, ActionContext& context) {
         return context.host.animationFinished(args[0]) ? kDone : kRunning;
     },
     [](const ActionArgs args, ActionContext& context) {
         return context.host.animationFinished(args[0]) ? kDone : kRunning;
     }},
    {"walk_to", "Anda ate o ponto de aproximacao do objeto e espera chegar", {objectId}, nullptr,
     [](const ActionArgs args, ActionContext& context) {
         return context.host.walkToObject(args[0]) ? kRunning : kDone;
     },
     [](ActionArgs, ActionContext& context) { return context.host.walking() ? kRunning : kDone; }},
    {"face", "Vira o player parado para left, right, up ou down", {facing}, nullptr,
     [](const ActionArgs args, ActionContext& context) {
         if (const std::optional<Facing> facing = parseFacing(args[0])) {
             context.host.facePlayer(*facing);
         }
         return kDone;
     },
     nullptr},
    {"wait", "Espera alguns segundos", {number}, nullptr,
     [](const ActionArgs args, ActionContext& context) {
         (void)parseNumberArgument(args[0], context.step.value);
         return context.step.value > 0.0F ? kRunning : kDone;
     },
     [](ActionArgs, ActionContext& context) {
         return context.step.elapsed >= context.step.value ? kDone : kRunning;
     }},
    // Blocks until the conversation ends; the dialogue's own steps run on top of this one.
    {"start_dialogue", "Abre um no de dialogo da sala e espera a conversa terminar",
     {dialogueId}, nullptr,
     [](const ActionArgs args, ActionContext& context) {
         return context.host.startDialogue(args[0]) ? kRunning : kDone;
     },
     [](ActionArgs, ActionContext& context) {
         return context.host.dialogueActive() ? kRunning : kDone;
     }},
    {"change_room",
     "Troca para <sala>.room quando a sequencia terminar; o player entra no [spawn] indicado "
     "(sem spawn: player.start)",
     ArgSignature({roomId, spawnId}, 1), nullptr,
     [](const ActionArgs args, ActionContext& context) {
         context.host.changeRoom(args[0], args.size() > 1 ? std::string_view(args[1])
                                                          : std::string_view());
         return kDone;
     },
     nullptr},
};

const ConditionDescriptor kConditions[]{
    {"flag_set", "A flag esta ativa", {flagId},
     [](const ActionArgs args, const SessionState& session, std::string_view) {
         return session.hasFlag(args[0]);
     }},
    {"flag_not_set", "A flag nao esta ativa", {flagId},
     [](const ActionArgs args, const SessionState& session, std::string_view) {
         return !session.hasFlag(args[0]);
     }},
    {"flag_at_least", "O contador da flag e maior ou igual a <n>", {flagId, integer},
     [](const ActionArgs args, const SessionState& session, std::string_view) {
         int minimum = 0;
         return parseIntegerArgument(args[1], minimum) && session.flagValue(args[0]) >= minimum;
     }},
    {"flag_equals", "O contador da flag e igual a <n> (ausente = 0)", {flagId, integer},
     [](const ActionArgs args, const SessionState& session, std::string_view) {
         int expected = 0;
         return parseIntegerArgument(args[1], expected) && session.flagValue(args[0]) == expected;
     }},
    {"has_item", "O item esta no inventario", {itemId},
     [](const ActionArgs args, const SessionState& session, std::string_view) {
         return session.hasItem(args[0]);
     }},
    {"lacks_item", "O item nao esta no inventario", {itemId},
     [](const ActionArgs args, const SessionState& session, std::string_view) {
         return !session.hasItem(args[0]);
     }},
    {"object_visible", "O objeto da sala esta visivel", {objectId},
     [](const ActionArgs args, const SessionState& session, const std::string_view roomId) {
         return session.isObjectVisible(roomId, args[0]);
     }},
    {"object_hidden", "O objeto da sala esta oculto", {objectId},
     [](const ActionArgs args, const SessionState& session, const std::string_view roomId) {
         return !session.isObjectVisible(roomId, args[0]);
     }},
};

template <typename Descriptor>
const Descriptor* findByKeyword(const std::span<const Descriptor> catalog,
                                const std::string_view keyword) {
    for (const Descriptor& entry : catalog) {
        if (entry.keyword == keyword) return &entry;
    }
    return nullptr;
}
} // namespace

std::string_view argPlaceholder(const ArgType type) {
    switch (type) {
    case objectId: return "<objeto>";
    case itemId: return "<item>";
    case flagId: return "<flag>";
    case roomId: return "<sala>";
    case animation: return "<animacao>";
    case text: return "\"texto\"";
    case boolean: return "<0|1>";
    case number: return "<segundos>";
    case dialogueId: return "<dialogo>";
    case spawnId: return "<spawn>";
    case integer: return "<n>";
    case facing: return "<left|right|up|down>";
    }
    return "<?>";
}

std::span<const EffectDescriptor> effectCatalog() { return kEffects; }

std::span<const ConditionDescriptor> conditionCatalog() { return kConditions; }

const EffectDescriptor* findEffect(const std::string_view keyword) {
    return findByKeyword(effectCatalog(), keyword);
}

const ConditionDescriptor* findCondition(const std::string_view keyword) {
    return findByKeyword(conditionCatalog(), keyword);
}

bool parseNumberArgument(const std::string_view text, float& value) {
    // Locale-independent: digits with an optional single '.' fraction, no sign or exponent.
    if (text.empty() || text.size() > 16) return false;
    double result = 0.0;
    double scale = 0.0;
    bool sawDigit = false;
    for (const char ch : text) {
        if (ch == '.') {
            if (scale != 0.0) return false;
            scale = 1.0;
            continue;
        }
        if (ch < '0' || ch > '9') return false;
        sawDigit = true;
        if (scale == 0.0) {
            result = result * 10.0 + (ch - '0');
        } else {
            scale *= 0.1;
            result += (ch - '0') * scale;
        }
    }
    if (!sawDigit || result > kMaxNumberArgument) return false;
    value = static_cast<float>(result);
    return true;
}

bool parseIntegerArgument(const std::string_view text, int& value) {
    // Locale-independent: optional '-', then digits only; no '+', spaces or fraction.
    std::string_view digits = text;
    const bool negative = digits.starts_with('-');
    if (negative) digits.remove_prefix(1);
    if (digits.empty() || digits.size() > 7) return false;
    int result = 0;
    for (const char ch : digits) {
        if (ch < '0' || ch > '9') return false;
        result = result * 10 + (ch - '0');
    }
    if (result > kMaxIntegerArgument) return false;
    value = negative ? -result : result;
    return true;
}

std::string actionUsage(const std::string_view keyword, const ArgSignature& signature) {
    std::string usage = "Use ";
    usage += keyword;
    const auto types = signature.view();
    for (std::size_t i = 0; i < types.size(); ++i) {
        const bool optional = i >= signature.required;
        usage += optional ? " [" : " ";
        usage += argPlaceholder(types[i]);
        if (optional) usage += ']';
    }
    return usage;
}

std::string argumentsError(const std::string_view keyword, const ArgSignature& signature,
                           const ActionArgs args, const Room* room) {
    const auto types = signature.view();
    if (!signature.accepts(args.size())) {
        return actionUsage(keyword, signature);
    }
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& value = args[i];
        switch (types[i]) {
        case text:
            if (value.empty()) return "Texto vazio em " + std::string(keyword);
            break;
        case boolean:
            if (value != "0" && value != "1") return actionUsage(keyword, signature);
            break;
        case number: {
            float parsed = 0.0F;
            if (!parseNumberArgument(value, parsed)) return "Numero invalido: " + value;
            break;
        }
        case integer: {
            int parsed = 0;
            if (!parseIntegerArgument(value, parsed)) return "Inteiro invalido: " + value;
            break;
        }
        case facing:
            if (!parseFacing(value)) {
                return "Direcao invalida: " + value + " (use left, right, up ou down)";
            }
            break;
        case objectId:
            if (!validIdentifier(value)) return "Identificador invalido: " + value;
            if (room && !room->findObject(value)) return "Objeto desconhecido na sala: " + value;
            break;
        case itemId:
        case flagId:
        case roomId:
        case animation:
        case dialogueId:
        case spawnId:
            if (!validIdentifier(value)) return "Identificador invalido: " + value;
            break;
        }
    }
    return {};
}

std::string effectArgumentsError(const EffectDescriptor& effect, const ActionArgs args,
                                 const Room* room) {
    std::string error = argumentsError(effect.keyword, effect.args, args, room);
    if (error.empty() && effect.validate) {
        error = effect.validate(args, room);
    }
    return error;
}
