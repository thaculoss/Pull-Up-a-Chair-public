#include "ServicePriority.h"
#include "ServiceControlPolicy.h"
#include "InnScenePolicy.h"
#include "ServiceScenePolicy.h"
#undef GetObject
namespace inn::service::priority {
namespace {
RE::TESPackage* Package(bool hold=false){auto data=RE::TESDataHandler::GetSingleton();return data?data->LookupForm<RE::TESPackage>(hold?0x830:0x82F,"Pull up a chair Ordering.esp"):nullptr;}
RE::AIProcess* Process(RE::Actor* actor){return actor?actor->GetActorRuntimeData().currentProcess:nullptr;}
bool Exists(RE::Actor* actor){return actor&&!actor->IsDeleted()&&!actor->IsMarkedForDeletion();}
bool OwnPackage(RE::TESPackage* package){return package&&(package==Package()||package==Package(true));}
RE::TESFaction* ServiceReservation(){auto data=RE::TESDataHandler::GetSingleton();return data?data->LookupForm<RE::TESFaction>(0x831,"Pull up a chair Ordering.esp"):nullptr;}
bool ChattyPatchEnabled(){
    auto data=RE::TESDataHandler::GetSingleton();auto file=data?data->LookupModByName("Pull up a chair - Chatty NPCs Patch.esp"):nullptr;
    return file&&file->compileIndex!=0xFF&&ServiceReservation();
}
bool ChattyScene(RE::BGSScene* scene){
    auto data=RE::TESDataHandler::GetSingleton();if(!data||!scene||!scene->parentQuest)return false;
    if(!ChattyPatchEnabled())return false;
    for(const auto& row:scenes::chatty){
        auto expected=data->LookupForm<RE::BGSScene>(row.scene,"Chatty NPCs.esp");
        auto quest=data->LookupForm<RE::TESQuest>(row.quest,"Chatty NPCs.esp");
        if(scene==expected&&scene->parentQuest==quest&&quest)return true;
    }
    return false;
}
bool InnRoutine(RE::TESPackage* package,RE::TESQuest* owner=nullptr){
    if(!package)return false;
    const auto quest=owner?owner:package->ownerQuest;
    if(quest&&package->ownerQuest==quest&&scenes::AllowedAliasPackage(package->GetFormID(),quest->GetFormID()))return true;
    // Use exact reviewed scene actions, never every package in a shared quest.
    static const auto packages=[](){
        std::vector<std::pair<RE::TESPackage*,RE::TESQuest*>> result;
        for(const auto& row:scenes::ambient)if(auto scene=RE::TESForm::LookupByID<RE::BGSScene>(row.id);scene&&AmbientScene(scene))
            for(auto action:scene->actions)if(action&&action->GetType()==RE::BGSSceneAction::Type::kPackage)
                for(auto value:static_cast<RE::BGSSceneActionPackage*>(action)->packages)if(value)result.emplace_back(value,scene->parentQuest);
        if(auto data=RE::TESDataHandler::GetSingleton())for(const auto& row:scenes::chatty)
            if(auto scene=data->LookupForm<RE::BGSScene>(row.scene,"Chatty NPCs.esp");ChattyScene(scene))
                for(auto action:scene->actions)if(action&&action->GetType()==RE::BGSSceneAction::Type::kPackage)
                    for(auto value:static_cast<RE::BGSSceneActionPackage*>(action)->packages)if(value)result.emplace_back(value,scene->parentQuest);
        return result;
    }();
    return std::ranges::any_of(packages,[&](const auto& entry){return entry.first==package&&
        (!owner||entry.second==owner)&&(!package->ownerQuest||entry.second==package->ownerQuest);});
}
control::Package Describe(RE::TESPackage* package){
    if(!package)return {};
    return {true,OwnPackage(package),package->packData.packFlags.any(RE::PACKAGE_DATA::GeneralFlag::kCreated),
        package->GetFile(0)!=nullptr,package->ownerQuest!=nullptr,package->GetFormID(),static_cast<unsigned>(package->packData.packType.get()),static_cast<unsigned>(package->procedureType.get()),InnRoutine(package)};
}
control::Slot Slot(RE::TESPackage* package){return control::Classify(Describe(package));}
bool PlayerDialogue(){auto ui=RE::UI::GetSingleton();return ui&&ui->IsMenuOpen("Dialogue Menu");}
control::Action Action(RE::Actor* actor){
    auto process=Process(actor);if(!process||!process->middleHigh)return control::Action::protectedControl;
    return control::Decide(Slot(process->middleHigh->runOncePackage.package),PlayerDialogue(),Scene(actor)!=nullptr);
}
}
void Trace(RE::Actor* actor,const char* reason){
    auto process=Process(actor);auto slot=process&&process->middleHigh?process->middleHigh->runOncePackage.package:nullptr;
    auto normal=process?process->currentPackage.package:nullptr;auto scene=Scene(actor);
    const auto slotTarget=process&&process->middleHigh?process->middleHigh->runOncePackage.target.get():RE::NiPointer<RE::TESObjectREFR>{};
    const auto normalTarget=process?process->currentPackage.target.get():RE::NiPointer<RE::TESObjectREFR>{};
    const auto file=slot?slot->GetFile(0):nullptr;
    logger::info("Service AI snapshot actor={:08X} reason={} slot={:08X} slot_type={} slot_procedure={} slot_kind={} generated={} source={} quest={:08X} slot_target={:08X} normal={:08X} normal_type={} normal_target={:08X} scene={:08X} player_dialogue={}",
        actor?actor->GetFormID():0,reason,slot?slot->GetFormID():0,slot?static_cast<int>(slot->packData.packType.get()):-1,slot?static_cast<int>(slot->procedureType.get()):-1,static_cast<int>(Slot(slot)),
        slot&&slot->packData.packFlags.any(RE::PACKAGE_DATA::GeneralFlag::kCreated),file?file->fileName:"<runtime>",slot&&slot->ownerQuest?slot->ownerQuest->GetFormID():0,
        slotTarget?slotTarget->GetFormID():0,normal?normal->GetFormID():0,normal?static_cast<int>(normal->packData.packType.get()):-1,normalTarget?normalTarget->GetFormID():0,scene?scene->GetFormID():0,PlayerDialogue());
}
RE::BGSScene* Scene(RE::Actor* actor){
    auto data=actor?actor->extraList.GetByType<RE::ExtraSceneData>():nullptr;
    return data&&data->scene&&(data->scene->isPlaying||data->scene->isShuttingDown)?data->scene:nullptr;
}
bool AmbientScene(RE::BGSScene* scene){
    if(!scene)return false;
    return scenes::Allowed(scene->GetFormID(),scene->parentQuest?scene->parentQuest->GetFormID():0)||ChattyScene(scene);
}
bool CanPrepare(RE::Actor* actor,bool prioritize){
    auto process=Process(actor);if(!Exists(actor)||!process||!process->middleHigh)return false;
    const auto scene=Scene(actor);const auto slot=process->middleHigh->runOncePackage.package;
    if(scene){
        // Stop only known vanilla inn scenes. A package belonging to a different
        // quest remains protected even while that vanilla scene is still active.
        return prioritize&&AmbientScene(scene)&&(!slot||Slot(slot)!=control::Slot::external||(slot->ownerQuest&&slot->ownerQuest==scene->parentQuest));
    }
    const auto kind=Slot(slot);
    return !PlayerDialogue()&&(kind==control::Slot::empty||kind==control::Slot::owned||
        (prioritize&&(kind==control::Slot::engineDialogue||kind==control::Slot::engineTransition||kind==control::Slot::innRoutine)));
}
bool StopAmbient(RE::Actor* actor){
    const auto scene=Scene(actor);if(!scene)return true;if(!AmbientScene(scene))return false;
    if(PlayerDialogue())return false;
    auto vm=RE::BSScript::Internal::VirtualMachine::GetSingleton();if(!vm)return false;
    // Native-started scenes need not already have a bound Papyrus Scene object.
    // Pack uses CommonLib's typed FindBoundObject/CreateObject/BindID path.
    RE::BSScript::Variable receiver;receiver.Pack(scene);
    auto object=receiver.IsObject()?receiver.GetObject():RE::BSTSmartPointer<RE::BSScript::Object>{};
    if(!object){logger::warn("Service scene stop binding failed actor={:08X} scene={:08X}",actor->GetFormID(),scene->GetFormID());return false;}
    struct Completion final:RE::BSScript::IStackCallbackFunctor {
        RE::FormID actor,scene;
        Completion(RE::FormID a,RE::FormID s):actor(a),scene(s){}
        void operator()(RE::BSScript::Variable)override{logger::info("Service Scene.Stop callback actor={:08X} scene={:08X}; engine shutdown still requires observation",actor,scene);}
        void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&)override{}
    };
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{new Completion(actor->GetFormID(),scene->GetFormID())};
    const bool sent=vm->DispatchMethodCall(object,"Stop",RE::MakeFunctionArguments(),callback);
    logger::info("Service scene stop dispatched actor={:08X} scene={:08X} compatibility={} bound=true accepted={}",actor->GetFormID(),scene->GetFormID(),ChattyScene(scene)?"Chatty NPCs":"vanilla inn",sent);
    return sent;
}
void ReserveForService(RE::Actor* actor){
    if(!actor)return;
    if(auto faction=ServiceReservation();faction&&!actor->IsInFaction(faction)){
        actor->AddToFaction(faction,0);
        logger::info("Service chatter reservation added actor={:08X}; reviewed vanilla banter and enabled Chatty NPCs patch defer new scene claims",actor->GetFormID());
    }
}
void ReportCompatibility(){
    logger::info("Service AI ownership guard rolled back; ordinary schedules and quest aliases no longer reserve staff; player dialogue and active scene/run-once control retained");
    auto data=RE::TESDataHandler::GetSingleton();auto chatty=data?data->LookupModByName("Chatty NPCs.esp"):nullptr;
    const bool present=chatty&&chatty->compileIndex!=0xFF;
    logger::info("Chatty NPCs compatibility present={} patch_enabled={} reviewed_scene_pairs={} reservation_form={:08X}",present,ChattyPatchEnabled(),scenes::chatty.size(),ServiceReservation()?ServiceReservation()->GetFormID():0);
    if(present&&!ChattyPatchEnabled())logger::warn("Chatty NPCs is active without the service compatibility patch; enable Pull up a chair - Chatty NPCs Patch.esp from the FOMOD");
}
bool ResetBefore(RE::Actor* actor){
    auto process=Process(actor);if(!Exists(actor)||actor->IsDead()||actor->IsInCombat()||!process||!process->middleHigh||Scene(actor))return false;
    const auto action=Action(actor);
    if(action==control::Action::protectedControl)return false;
    // Getting out of furniture is an engine transition, not another mod's lease.
    // Let it complete and keep this service request alive.
    if(action==control::Action::waitTransition)return true;
    Trace(actor,"before service preparation");
    process->StopCurrentIdle(actor,true);
    // false means include NPC dialogue interrupts. Never call this during an
    // actual player dialogue, and never EvaluatePackage before installing Travel:
    // EVP would choose a daily/hello package in the inter-frame reset window.
    actor->EndDialogue();actor->EndInterruptPackage(false);
    process=Process(actor);if(!process||!process->middleHigh)return false;
    const auto kind=Slot(process->middleHigh->runOncePackage.package);
    if(kind==control::Slot::external)return false;
    if(kind==control::Slot::owned||kind==control::Slot::engineDialogue||kind==control::Slot::innRoutine)process->SetRunOncePackage(nullptr,actor);
    process->ClearActionHeadtrackTarget(false);
    logger::info("Service prepared actor={:08X}; no daily EVP, Travel installed in the same update",actor->GetFormID());
    return true;
}
Result Acquire(RE::Actor* actor,bool hold) {
    auto own=Package(hold);if(!own)return Result::missing;
    auto process=Process(actor);if(!Exists(actor)||actor->IsDead()||!process||!process->middleHigh)return Result::invalid;
    auto slot=process->middleHigh->runOncePackage.package;
    auto action=Action(actor);
    if(slot==own&&action==control::Action::keep)return Result::ready;
    if(action==control::Action::protectedControl)return Result::busy;
    if(action==control::Action::waitTransition)return Result::transition;
    if(action==control::Action::interruptDialogue){
        Trace(actor,"reclaiming service from engine NPC dialogue");
        actor->EndDialogue();actor->EndInterruptPackage(false);
        process=Process(actor);if(!process||!process->middleHigh)return Result::invalid;
        action=Action(actor);
        if(action==control::Action::protectedControl)return Result::busy;
        if(action==control::Action::waitTransition)return Result::transition;
        slot=process->middleHigh->runOncePackage.package;
        if(Slot(slot)==control::Slot::engineDialogue||Slot(slot)==control::Slot::owned||Slot(slot)==control::Slot::innRoutine)process->SetRunOncePackage(nullptr,actor);
    }
    process->SetRunOncePackage(own,actor);
    const bool accepted=process->middleHigh&&process->middleHigh->runOncePackage.package==own;
    // The setter schedules processing. Never EvaluatePackage here.
    logger::info("Service package installed actor={:08X} package={:08X} hold={} accepted={}; no post-install EVP",actor->GetFormID(),own->GetFormID(),hold,accepted);
    Trace(actor,"after service package installation");
    return accepted?Result::ready:Result::invalid;
}
bool Owns(RE::Actor* actor){auto process=Process(actor);return process&&process->middleHigh&&OwnPackage(process->middleHigh->runOncePackage.package);}
bool NeedsResume(RE::Actor* actor){
    auto process=Process(actor);if(!process||!process->middleHigh)return true;
    return !OwnPackage(process->middleHigh->runOncePackage.package);
}
bool RecheckOwnedTravel(RE::Actor* actor){
    auto process=Process(actor);auto travel=Package(false);
    if(!Exists(actor)||actor->IsDead()||actor->IsInCombat()||!travel||!process||!process->middleHigh||
       process->middleHigh->runOncePackage.package!=travel||PlayerDialogue()||Scene(actor))return false;
    // This queues the engine's ordinary evaluation and explicitly clears the
    // reset-AI request. It is not the immediate/reset EVP that dropped Travel.
    // Verified against the local 1.5.97 function; the runtime-aware library API
    // supplies the corresponding relocation on other supported versions.
    actor->EvaluatePackage(false,false);
    process=Process(actor);
    const bool retained=process&&process->middleHigh&&process->middleHigh->runOncePackage.package==travel;
    const bool queued=actor->GetActorRuntimeData().boolBits.any(RE::Actor::BOOL_BITS::kEvpBuffered);
    logger::info("Service queued non-reset recheck actor={:08X} own_travel_retained={} queued={}",actor->GetFormID(),retained,queued);
    Trace(actor,"queued non-reset travel recheck");
    return retained&&queued;
}
void Release(RE::Actor* actor,bool reset) {
    // Clear our own marker even if another quest took over the package slot.
    if(actor)if(auto faction=ServiceReservation();faction&&actor->IsInFaction(faction)){
        actor->RemoveFromFaction(faction);logger::info("Service chatter reservation cleared actor={:08X}",actor->GetFormID());
    }
    auto process=Process(actor);if(!Exists(actor)||!process)return;
    if(process->middleHigh&&process->middleHigh->runOncePackage.package&&!OwnPackage(process->middleHigh->runOncePackage.package)){
        Trace(actor,"release preserves non-owned control");return;
    }
    const bool owned=process->middleHigh&&OwnPackage(process->middleHigh->runOncePackage.package);
    if(owned)process->SetRunOncePackage(nullptr,actor);
    if(PlayerDialogue()){
        // Releasing our slot must not close an actual player conversation.
        if(owned&&!actor->IsDead())actor->EvaluatePackage(false,false);
        Trace(actor,"service released without resetting player dialogue");return;
    }
    if(reset&&!actor->IsDead()&&!actor->IsInCombat()&&!Scene(actor)){
        process->StopCurrentIdle(actor,true);process->ClearActionHeadtrackTarget(false);actor->EndInterruptPackage(true);actor->EvaluatePackage(true,true);
    }
    logger::info("Service package released actor={:08X} reset={}; normal package list preserved",actor->GetFormID(),reset);
}
}
