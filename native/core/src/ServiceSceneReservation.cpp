#include "PCH.h"
#include "ServiceSceneReservation.h"
#include "ServiceSceneReservationPolicy.h"
#include "RE/B/BGSStoryEventManager.h"
#include "RE/B/BGSStoryManagerEventNode.h"

namespace inn::service::reservation {
namespace {
std::optional<ActorFields> RuntimeActorFields(){
    const auto manager=RE::BGSStoryEventManager::GetSingleton();
    if(!manager||manager->registeredEvents.empty()||manager->registeredEvents.size()>256)return std::nullopt;
    const RE::BGSRegisteredStoryEvent* event=nullptr;
    for(std::uint32_t index=0;index<manager->registeredEvents.size();++index){
        const auto& candidate=manager->registeredEvents[index];
        if(std::string_view(candidate.uniqueID,4)!="ADIA")continue;
        if(event||index!=static_cast<std::uint32_t>(RE::QuestEvent::kActorDialogue))return std::nullopt;
        event=&candidate;
    }
    if(!event||!event->members||event->members->empty()||event->members->size()>64)return std::nullopt;
    std::vector<EventField> fields;
    for(const auto& member:*event->members)
        fields.push_back({member.uniqueID,member.type.get()==RE::BGSStoryEventMember::DATA_TYPE::Reference});
    const auto result=ResolveActorFields(fields);
    if(result)logger::info("Vanilla inn reservation field map revision=2 event=ADIA first_tag={:04X} first_index={} second_tag={:04X} second_index={} members={}; selectors resolved from engine registry",firstActorTag,result->first,secondActorTag,result->second,fields.size());
    return result;
}
bool ValidChain(const RE::TESCondition* conditions){
    if(!conditions||!conditions->head)return false;
    unsigned count=0;
    for(auto item=conditions->head;item;item=item->next)if(++count>256)return false;
    return true;
}
RE::BGSRefAlias* Candidate(RE::TESQuest* quest,std::uint32_t id,std::uint32_t selector){
    RE::BGSRefAlias* result=nullptr;
    for(auto base:quest->aliases){
        if(!base||base->aliasID!=id)continue;
        const auto vmType=base->GetVMTypeID();
        const char* reason=result?"duplicate_alias":base->owningQuest!=quest?"wrong_owner":
            vmType!=RE::BGSRefAlias::VMTYPEID?"not_reference_alias":
            base->fillType!=RE::BGSBaseAlias::FILL_TYPE::kFromEvent?"not_event_fill":nullptr;
        if(reason){
            logger::warn("Vanilla inn reservation alias rejected quest={:08X} alias={} reason={} owner_matches={} vm_type={} fill_type={}",quest->GetFormID(),id,reason,base->owningQuest==quest,static_cast<unsigned>(vmType),static_cast<unsigned>(base->fillType.get()));
            return nullptr;
        }
        auto alias=static_cast<RE::BGSRefAlias*>(base);
        const auto event=alias->fillData.fromEvent.forcedFromEvent;
        const auto actual=alias->fillData.fromEvent.forcedEventData;
        const bool optional=alias->flags.any(RE::BGSBaseAlias::FLAGS::kOptional),valid=ValidChain(alias->conditions);
        if(!CandidateShape(true,true,event==RE::QuestEvent::kActorDialogue,optional,valid,actual,selector)){
            reason=event!=RE::QuestEvent::kActorDialogue?"wrong_event":optional?"optional_alias":!valid?"invalid_conditions":"wrong_runtime_selector";
            logger::warn("Vanilla inn reservation alias rejected quest={:08X} alias={} reason={} event={} runtime_selector={} expected_selector={} optional={} conditions_valid={}",quest->GetFormID(),id,reason,static_cast<unsigned>(event),actual,selector,optional,valid);
            return nullptr;
        }
        result=alias;
    }
    if(!result)logger::warn("Vanilla inn reservation alias rejected quest={:08X} alias={} reason=missing_alias",quest->GetFormID(),id);
    return result;
}
bool Matches(const RE::TESConditionItem& item,RE::TESFaction* faction){
    const auto& data=item.data;
    return data.functionData.function==RE::FUNCTION_DATA::FunctionID::kGetInFaction&&
        data.functionData.params[0]==faction&&!data.functionData.params[1]&&
        data.object==RE::CONDITIONITEMOBJECT::kSelf&&data.comparisonValue.f==0.f&&
        !data.flags.isOR&&!data.flags.usesAliases&&!data.flags.global&&!data.flags.usePackData&&
        !data.flags.swapTarget&&data.flags.opCode==RE::CONDITION_ITEM_DATA::OpCode::kEqualTo;
}
bool Install(RE::BGSRefAlias* alias,RE::TESFaction* faction){
    return PrefixOnce(alias->conditions->head,[&](const auto& item){return Matches(item,faction);},[&](){
        auto item=new RE::TESConditionItem();
        item->data.functionData.function=RE::FUNCTION_DATA::FunctionID::kGetInFaction;
        item->data.functionData.params[0]=faction;
        item->data.comparisonValue.f=0.f;
        item->data.flags.isOR=false;
        item->data.flags.opCode=RE::CONDITION_ITEM_DATA::OpCode::kEqualTo;
        item->data.object=RE::CONDITIONITEMOBJECT::kSelf;
        return item;
    });
}
}
void Initialize(){
    const auto data=RE::TESDataHandler::GetSingleton();
    auto faction=data?data->LookupForm<RE::TESFaction>(0x831,"Pull up a chair Ordering.esp"):nullptr;
    if(!faction){logger::error("Vanilla inn service reservation unavailable: core faction 831 missing");return;}
    const auto fields=RuntimeActorFields();
    if(!fields){logger::error("Vanilla inn service reservation unavailable: ActorDialogue reference field mapping missing or ambiguous; no aliases modified");return;}
    unsigned added=0,ready=0,skipped=0;
    for(const auto& row:definitions){
        auto scene=RE::TESForm::LookupByID<RE::BGSScene>(row.scene);
        auto quest=RE::TESForm::LookupByID<RE::TESQuest>(row.quest);
        if(!scene||!quest||scene->parentQuest!=quest||quest->eventID!=RE::QuestEvent::kActorDialogue){
            ++skipped;logger::warn("Vanilla inn reservation skipped scene={:08X} quest={:08X}: final scene parent or story event differs",row.scene,row.quest);continue;
        }
        auto first=Candidate(quest,row.firstAlias,fields->first);
        auto second=Candidate(quest,row.secondAlias,fields->second);
        // Validate both event candidates before changing either. Unique actors,
        // optional candidates, and altered non-ADIA aliases are never modified.
        if(!first||!second){
            ++skipped;logger::warn("Vanilla inn reservation skipped quest={:08X}: final event alias shape differs",row.quest);continue;
        }
        added+=Install(first,faction)?1U:0U;added+=Install(second,faction)?1U:0U;ready+=2;
    }
    logger::info("Vanilla inn service reservation ready_aliases={} added={} reviewed_quests={} skipped_quests={}; only reserved waiter excluded from new ambient ActorDialogue events; WITavern aliases unchanged",ready,added,definitions.size(),skipped);
}
}
