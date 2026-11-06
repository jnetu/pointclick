#include "game/ActionCatalog.hpp"

#include "game/Identifier.hpp"
#include "game/Room.hpp"
#include "game/SessionState.hpp"

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

const EffectDescriptor kEffects[]{
    {"say", "Exibe a fala/observacao na tela", {text}, nullptr,
     [](const ActionArgs args, EffectContext& context) {
         context.events.emplace_back(SayEvent{args[0]});
     }},
    {"set_flag", "Ativa uma flag na sessao", {flagId}, nullptr,
     [](const ActionArgs args, EffectContext& context) { context.session.setFlag(args[0]); }},
    {"clear_flag", "Limpa uma flag na sessao", {flagId}, nullptr,
     [](const ActionArgs args, EffectContext& context) { context.session.clearFlag(args[0]); }},
    {"give_item", "Adiciona um item ao inventario", {itemId}, nullptr,
     [](const ActionArgs args, EffectContext& context) { context.session.addItem(args[0]); }},
    {"take_item", "Remove um item do inventario", {itemId}, nullptr,
     [](const ActionArgs args, EffectContext& context) { context.session.removeItem(args[0]); }},
    {"hide_object", "Esconde um objeto da sala", {objectId}, nullptr,
     [](const ActionArgs args, EffectContext& context) {
         context.session.setObjectVisible(context.roomId, args[0], false);
         context.navigationChanged = true;
     }},
    {"show_object", "Revela um objeto da sala", {objectId}, nullptr,
     [](const ActionArgs args, EffectContext& context) {
         context.session.setObjectVisible(context.roomId, args[0], true);
         context.navigationChanged = true;
     }},
    {"set_solid", "Liga (1) ou desliga (0) a colisao e reconstroi a navegacao",
     {objectId, boolean}, nullptr,
     [](const ActionArgs args, EffectContext& context) {
         context.session.setObjectSolid(context.roomId, args[0], args[1] == "1");
         context.navigationChanged = true;
     }},
    {"play_animation", "Toca/reinicia um clip configurado no objeto", {objectId, animation},
     validateObjectAnimation,
     [](const ActionArgs args, EffectContext& context) {
         context.events.emplace_back(PlayAnimationEvent{args[0], args[1]});
     }},
    {"change_room", "Solicita transicao para <sala>.room", {roomId}, nullptr,
     [](const ActionArgs args, EffectContext& context) {
         context.events.emplace_back(ChangeRoomEvent{args[0]});
     }},
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

std::string actionUsage(const std::string_view keyword, const ArgSignature& signature) {
    std::string usage = "Use ";
    usage += keyword;
    for (const ArgType type : signature.view()) {
        usage += ' ';
        usage += argPlaceholder(type);
    }
    return usage;
}

std::string argumentsError(const std::string_view keyword, const ArgSignature& signature,
                           const ActionArgs args, const Room* room) {
    const auto types = signature.view();
    if (args.size() != types.size()) {
        return actionUsage(keyword, signature);
    }
    for (std::size_t i = 0; i < types.size(); ++i) {
        const std::string& value = args[i];
        switch (types[i]) {
        case text:
            if (value.empty()) return "Texto vazio em " + std::string(keyword);
            break;
        case boolean:
            if (value != "0" && value != "1") return actionUsage(keyword, signature);
            break;
        case objectId:
            if (!validIdentifier(value)) return "Identificador invalido: " + value;
            if (room && !room->findObject(value)) return "Objeto desconhecido na sala: " + value;
            break;
        case itemId:
        case flagId:
        case roomId:
        case animation:
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
