#include "PCH.h"
#include "TopicVisibility.h"
#include "CallService.h"
#include "InnServices.h"

namespace inn::topics {
namespace {
constexpr auto plugin="Pull up a chair Ordering.esp";
struct Gate {
    RE::FormID infoLocal,topicLocal;
    const char* name;
    RE::TESGlobal* global{};
    RE::TESConditionItem* condition{};
};
std::array gates{Gate{0x80B,0x80A,"PullUpAChairHideClearDialogue"},
                 Gate{0x80E,0x80D,"PullUpAChairHideRestoreDialogue"}};
bool installed{};

bool Install(Gate& gate) {
    auto data=RE::TESDataHandler::GetSingleton();
    auto info=data?data->LookupForm<RE::TESTopicInfo>(gate.infoLocal,plugin):nullptr;
    auto topic=data?data->LookupForm<RE::TESTopic>(gate.topicLocal,plugin):nullptr;
    if(!info||!topic||info->parentTopic!=topic){
        logger::error("Cleanup dialogue visibility refused mismatched records info={:03X} topic={:03X}",gate.infoLocal,gate.topicLocal);
        return false;
    }
    if(gate.condition)return true;
    // Factory-created private globals are held for the process lifetime. They
    // are not assigned IDs, registered, serialized, or added to the ESP. The
    // engine condition reads the pointer directly; INI settings remain authority.
    gate.global=RE::IFormFactory::Create<RE::TESGlobal>();
    if(!gate.global){logger::error("Cleanup dialogue visibility global creation failed info={:03X}",gate.infoLocal);return false;}
    gate.global->type=RE::TESGlobal::Type::kShort;
    gate.global->value=0.f;
    gate.global->SetFormEditorID(gate.name);
    gate.condition=new RE::TESConditionItem();
    auto& condition=gate.condition->data;
    condition.functionData.function=RE::FUNCTION_DATA::FunctionID::kGetGlobalValue;
    condition.functionData.params[0]=gate.global;
    condition.comparisonValue.f=0.f;
    condition.flags.isOR=false;
    condition.flags.opCode=RE::CONDITION_ITEM_DATA::OpCode::kEqualTo;
    // Prepend an AND guard, outside the existing owner's OR server group.
    gate.condition->next=info->objConditions.head;
    info->objConditions.head=gate.condition;
    logger::info("Cleanup dialogue visibility guard installed info={:08X} topic={:08X}; existing conditions and fragment retained",info->GetFormID(),topic->GetFormID());
    return true;
}
void Sync() {
    if(!installed)return;
    const auto settings=call::GetSettings();
    gates[0].global->value=settings.hideClearTableDialogue?1.f:0.f;
    gates[1].global->value=settings.hideRestoreTableDialogue?1.f:0.f;
    logger::info("Cleanup dialogue visibility hide_clear={} hide_restore={}; applied for next dialogue evaluation",settings.hideClearTableDialogue,settings.hideRestoreTableDialogue);
}
}
void Initialize() {
    // Called after data loading and settings read, on the game's message thread.
    const bool clear=Install(gates[0]),restore=Install(gates[1]);
    installed=clear&&restore;
    Sync();
}
void QueueSync() {
    // UI drawing may run off the game thread. Read the latest saved settings in
    // the queued task so rapid toggles cannot apply an obsolete copied value.
    SKSE::GetTaskInterface()->AddTask(Sync);
}
void TraceDialogueContext(){
    static RE::FormID lastSpeaker{},lastSeat{};
    const auto ui=RE::UI::GetSingleton();
    if(!ui||!ui->IsMenuOpen("Dialogue Menu")){lastSpeaker=lastSeat=0;return;}
    const auto manager=RE::MenuTopicManager::GetSingleton();
    const auto ref=manager?manager->speaker.get():RE::NiPointer<RE::TESObjectREFR>{};
    const auto actor=ref?ref->As<RE::Actor>():nullptr;
    const auto player=RE::PlayerCharacter::GetSingleton();if(!actor||!player)return;
    const auto seat=player->GetOccupiedFurniture().get();const auto sid=seat?seat->GetFormID():0;
    if(lastSpeaker==actor->GetFormID()&&lastSeat==sid)return;
    lastSpeaker=actor->GetFormID();lastSeat=sid;
    const auto data=RE::TESDataHandler::GetSingleton();
    const auto quest=data?data->LookupForm<RE::TESQuest>(0x805,plugin):nullptr;
    const auto info=data?data->LookupForm<RE::TESTopicInfo>(0x808,plugin):nullptr;
    RE::TESConditionItem offers;
    offers.data.functionData.function=RE::FUNCTION_DATA::FunctionID::kGetOffersServicesNow;
    offers.data.comparisonValue.f=1.f;
    offers.data.flags.opCode=RE::CONDITION_ITEM_DATA::OpCode::kEqualTo;
    RE::ConditionCheckParams context{actor,player};context.quest=quest;
    const auto inn=FindInn(actor),playerInn=FindInn(player);
    logger::info("Dining dialogue context actor={:08X} quest_running={} order_conditions={} seated={} seat={:08X} same_cell={} inn={:08X} player_inn={:08X} offers_services_now={}; offers flag is diagnostic only in innfix1",
        actor->GetFormID(),quest&&quest->IsRunning(),info&&info->objConditions.IsTrue(actor,player),IsSeated(player),sid,
        actor->GetParentCell()==player->GetParentCell(),inn?inn->GetFormID():0,playerInn?playerInn->GetFormID():0,offers.IsTrue(context));
}
}
