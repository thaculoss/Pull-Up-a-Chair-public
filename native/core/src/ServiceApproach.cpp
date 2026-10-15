#include "ServiceApproach.h"
#include "ServicePolicy.h"
#include "InnServices.h"
#include "Localization.h"
#include "CallService.h"
#include "ServicePriority.h"
#include "TableCleanup.h"
#include "TableManager.h"
#include "RecordedSounds.h"
#include "ServiceLease.h"
#include "ServiceSelection.h"
#include "ServiceControlPolicy.h"
#include "ServiceScenePolicy.h"
#include "SupplierServices.h"
#include "InnServiceBackend.h"
namespace inn::service {
namespace {
namespace api=innservice::api;
constexpr float arrivalDistance=220.f;
struct Outcome {std::atomic_bool done{},success{};std::atomic_uint64_t finishedAt{};};
struct RequestState {
    RE::ObjectRefHandle server,seat;RE::FormID cell{},inn{};
    Phase phase=Phase::preparing;float elapsed{},stage{},lost{},callElapsed{},controlWait{};
    std::chrono::steady_clock::time_point tick{};std::shared_ptr<Outcome> outcome;
    cleanup::Target table;Motion motion;
    unsigned recoveries{},pathRetries{},cleanAttempts{},interruptions{};std::uint32_t order{};
    std::vector<RE::FormID> attempted;
    int controlKind{-1};
    scenes::StopRequest sceneStop;
    bool prioritize=true,autoClear{},cleaned{},resetIssued{},dialogueSeen{},waitingNotice{},fromCall{};
};
RequestState current;std::atomic_bool active{},busy{},repeatBell{};std::vector<RE::FormID> recoverActors;
Lease lease{(GetTickCount()&0x1FFFFFFF)+1};
std::atomic_bool dataReady{},legacyReady{};
std::atomic_uint64_t generation{1};
CleanupVisit visit;cleanup::Target visitTable;
api::Service SnapshotService(){
    api::Service result;if(!active||!lease.token)return result;
    result.token=lease.token;
    const auto ref=current.server.get(),seat=current.seat.get();
    result.server=ref?ref->GetFormID():0;result.seat=seat?seat->GetFormID():0;result.inn=current.inn;
    switch(current.phase){
        case Phase::preparing:result.phase=static_cast<unsigned>(api::Phase::starting);break;
        case Phase::approaching:result.phase=static_cast<unsigned>(api::Phase::approaching);break;
        case Phase::talking:case Phase::ordering:result.phase=static_cast<unsigned>(api::Phase::conversation);break;
        case Phase::releasing:result.phase=static_cast<unsigned>(api::Phase::releasing);break;
    }
    const auto supply=suppliers::Resolve(ref?ref->As<RE::Actor>():nullptr);
    result.merchant=supply.id.merchant;result.chest=supply.id.stock;return result;
}
void QueueEvent(std::uint32_t type,api::Service payload){
    const auto epoch=generation.load();
    // Never call consumers while an order or supplier mutex is held. A copied
    // value also prevents callbacks from retaining pointers into engine state.
    SKSE::GetTaskInterface()->AddTask([epoch,type,payload]()mutable{
        if(generation.load()!=epoch)return;
        if(type==api::serviceReady&&(!active||lease.token!=payload.token||current.phase==Phase::releasing))return;
        SKSE::GetMessagingInterface()->Dispatch(type,&payload,sizeof(payload),nullptr);
    });
}
void NotifyReady(){if(lease.MarkReady())QueueEvent(api::serviceReady,SnapshotService());}
void InvalidateBackend(){++generation;lease.Invalidate();legacyReady=GetModuleHandleW(L"PullUpAChairService.dll")==nullptr;}
bool ProtectPaidMeal(const cleanup::Target& target){
    std::vector<RE::FormID> tableIDs;for(const auto& region:target.regions)tableIDs.push_back(region.first);
    if(!tables::HasCommittedForTable(visit.seat,visit.cell,tableIDs))return false;
    visit.Seal();logger::info("Automatic order cleanup protected existing paid meal seat={:08X} cell={:08X} table_regions={}",visit.seat,visit.cell,target.regions.size());return true;
}
void ObserveSeat(RE::PlayerCharacter* player){
    const auto inn=FindInn(player);auto seat=player?player->GetOccupiedFurniture().get():RE::NiPointer<RE::TESObjectREFR>{};
    if(!IsSeated(player)||!seat||!player->GetParentCell()||!inn){
        if(visit.Observe(0,0,0))visitTable={};return;
    }
    if(visit.Observe(seat->GetFormID(),player->GetParentCell()->GetFormID(),inn->GetFormID())){
        // Capture before a vanilla greeting has a chance to turn the player's head.
        visitTable=cleanup::Capture();
        ProtectPaidMeal(visitTable);
    }
}
bool Exists(RE::TESObjectREFR* ref){return ref&&!ref->IsDeleted()&&!ref->IsMarkedForDeletion()&&!ref->IsDisabled();}
bool Eligible(RE::Actor* actor,RE::PlayerCharacter* player){
    if(!Exists(actor)||!player||actor==player||actor->IsDead()||actor->IsInCombat()||!actor->Is3DLoaded()||actor->GetParentCell()!=player->GetParentCell()||FindInn(actor)!=FindInn(player))return false;
    const auto owner=RE::TESForm::LookupByID<RE::TESFaction>(0x5091B),staff=RE::TESForm::LookupByID<RE::TESFaction>(0xDEE93);
    return (owner&&actor->IsInFaction(owner))||(staff&&actor->IsInFaction(staff));
}
selection::Role RoleOf(RE::Actor* actor){
    const auto owner=RE::TESForm::LookupByID<RE::TESFaction>(0x5091B),staff=RE::TESForm::LookupByID<RE::TESFaction>(0xDEE93);
    if(actor&&owner&&actor->IsInFaction(owner))return selection::Role::owner;
    return actor&&staff&&actor->IsInFaction(staff)?selection::Role::waiter:selection::Role::none;
}
bool CallAvailable(RE::Actor* actor,RE::PlayerCharacter* player,bool prioritize){
    if(!Eligible(actor,player)||actor->IsPlayerTeammate()||actor->IsHostileToActor(player))return false;
    const auto state=actor->AsActorState()->GetSitSleepState();
    if(state==RE::SIT_SLEEP_STATE::kIsSleeping||state==RE::SIT_SLEEP_STATE::kWantToSleep)return false;
    return priority::CanPrepare(actor,prioritize);
}
RE::Actor* SelectServer(RE::PlayerCharacter* player,bool prioritize,std::span<const RE::FormID> attempted={},bool trace=false){
    auto cell=player?player->GetParentCell():nullptr;if(!cell)return nullptr;
    std::vector<selection::Candidate> candidates;
    cell->ForEachReference([&](RE::TESObjectREFR* ref){
        const auto actor=ref?ref->As<RE::Actor>():nullptr;if(!Eligible(actor,player))return RE::BSContainer::ForEachResult::kContinue;
        const bool available=CallAvailable(actor,player,prioritize);
        candidates.push_back({actor->GetFormID(),RoleOf(actor),actor->GetPosition().GetSquaredDistance(player->GetPosition()),available});
        if(trace&&std::ranges::find(attempted,actor->GetFormID())==attempted.end())priority::Trace(actor,available?"call candidate available":"call candidate protected or unavailable");
        return RE::BSContainer::ForEachResult::kContinue;
    });const auto chosen=selection::Choose(candidates,attempted);
    return chosen?RE::TESForm::LookupByID<RE::Actor>(chosen->id):nullptr;
}
void Finish(const char* message=nullptr,bool reset=true){
    if(active&&lease.token){auto ended=SnapshotService();ended.phase=static_cast<unsigned>(api::Phase::releasing);QueueEvent(api::serviceEnded,ended);}
    lease.End();
    auto ref=current.server.get();const bool owned=current.fromCall;active=false;busy=false;repeatBell=false;current={};
    if(owned)priority::Release(ref?ref->As<RE::Actor>():nullptr,reset);
    if(message)RE::SendHUDMessage::ShowHUDMessage(T(message));
}
void Initialize(RE::Actor* server,RE::PlayerCharacter* player,bool fromCall=false){
    current={};current.server=server->GetHandle();current.seat=player->GetOccupiedFurniture();
    current.cell=player->GetParentCell()->GetFormID();current.inn=FindInn(player)->GetFormID();
    current.tick=std::chrono::steady_clock::now();current.outcome=std::make_shared<Outcome>();current.fromCall=fromCall;
    const auto options=call::GetSettings();current.prioritize=options.prioritizeService;current.autoClear=options.autoClearBeforeTalk;
    current.table=fromCall?cleanup::Capture():visitTable;
    if(current.table.regions.empty())current.table=cleanup::Capture();
    const auto p=server->GetPosition();current.motion.Reset(p.x,p.y,p.z);active=true;busy=true;repeatBell=fromCall;
    lease.Begin();
    if(fromCall&&current.prioritize)priority::ReserveForService(server);
    if(fromCall)current.attempted.push_back(server->GetFormID());
}
struct Completion:RE::BSScript::IStackCallbackFunctor {
    explicit Completion(std::shared_ptr<Outcome> value):state(std::move(value)){}
    std::shared_ptr<Outcome> state;
    void operator()(RE::BSScript::Variable value)override{state->success=value.IsBool()&&value.GetBool();state->finishedAt=GetTickCount64();state->done=true;}
    void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&)override{}
};
bool LegacyPath(RE::Actor* server){
    if(!CanWalkForService(server))return false;
    auto quest=RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESQuest>(0x805,"Pull up a chair Ordering.esp");
    auto vm=RE::BSScript::Internal::VirtualMachine::GetSingleton();auto policy=vm?vm->GetObjectHandlePolicy():nullptr;if(!quest||!policy)return false;
    current.outcome=std::make_shared<Outcome>();RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{new Completion(current.outcome)};
    return vm->DispatchMethodCall(policy->GetHandleForObject(RE::TESQuest::FORMTYPE,quest),"PullUpAChairOrderingQuest","WalkForService",RE::MakeFunctionArguments(static_cast<RE::Actor*>(server)),callback);
}
bool TryNextServer(const char* reason){
    if(!selection::CanFallback(current.fromCall,current.phase,static_cast<unsigned>(current.attempted.size()),current.callElapsed))return false;
    const auto player=RE::PlayerCharacter::GetSingleton();
    const auto inn=FindInn(player);
    if(!player||!IsSeated(player)||player->IsInCombat()||!player->GetParentCell()||!inn||
       player->GetOccupiedFurniture()!=current.seat||player->GetParentCell()->GetFormID()!=current.cell||inn->GetFormID()!=current.inn)return false;
    auto next=SelectServer(player,current.prioritize,current.attempted,true);if(!next)return false;
    const auto previous=current.server.get();const auto old=previous?previous->As<RE::Actor>():nullptr;
    if(old)priority::Release(old,false);
    current.server=next->GetHandle();current.attempted.push_back(next->GetFormID());
    current.phase=Phase::preparing;current.elapsed=0;current.stage=0;current.lost=0;
    current.recoveries=0;current.pathRetries=0;current.sceneStop={};current.interruptions=0;
    current.resetIssued=false;current.waitingNotice=false;current.dialogueSeen=false;
    current.controlWait=0;current.controlKind=-1;
    current.outcome=std::make_shared<Outcome>();current.tick=std::chrono::steady_clock::now();
    const auto p=next->GetPosition();current.motion.Reset(p.x,p.y,p.z);
    repeatBell=true;busy=true;
    if(current.prioritize)priority::ReserveForService(next);
    RE::SendHUDMessage::ShowHUDMessage(T("call_approaching"));
    logger::info("Service fallback previous={:08X} next={:08X} role={} reason={} attempt={} token={}; same bell and table retained",old?old->GetFormID():0,next->GetFormID(),RoleOf(next)==selection::Role::waiter?"waiter":"owner",reason,current.attempted.size(),lease.token);
    return true;
}
void FailApproach(const char* message,const char* reason){
    if(TryNextServer(reason))return;
    const auto ref=current.server.get();auto server=ref?ref->As<RE::Actor>():nullptr;
    logger::warn("Service approach ended actor={:08X} reason={} phase={} elapsed={} total={}",server?server->GetFormID():0,reason,static_cast<int>(current.phase),current.elapsed,current.callElapsed);
    priority::Trace(server,reason);Finish(message);
}
void WaitForControl(RE::Actor* server,priority::Result result,const char* reason,float dt){
    // An actual external quest/scene can hand service to the owner. Engine
    // dialogue/furniture transitions keep this request alive rather than making
    // the player reset AI by repeatedly ringing the bell.
    if(result==priority::Result::busy&&TryNextServer(reason))return;
    const auto kind=static_cast<int>(result);
    if(current.controlKind!=kind){
        current.controlKind=kind;
        priority::Trace(server,reason);
        logger::info("Service control wait actor={:08X} reason={} token={}; repeated bell preserves request",server->GetFormID(),reason,lease.token);
    }
    current.controlWait+=dt;current.lost=0;
    const auto p=server->GetPosition();current.motion.Reset(p.x,p.y,p.z);
    const auto action=result==priority::Result::transition?control::Action::waitTransition:control::Action::protectedControl;
    if(control::WaitExpired(action,current.controlWait))FailApproach("call_dispatch_failed",reason);
}
void RecoverPath(RE::Actor* server,const char* reason){
    const auto action=control::ChooseRecovery(current.recoveries,priority::Owns(server),current.motion.still>=stuckDelay);
    const auto player=RE::PlayerCharacter::GetSingleton();const auto p=server->GetPosition();
    if(player){const auto goal=player->GetPosition();logger::info("Service movement recovery actor={:08X} reason={} action={} actor_position={:.1f},{:.1f},{:.1f} player_position={:.1f},{:.1f},{:.1f} distance={:.1f}",server->GetFormID(),reason,static_cast<int>(action),p.x,p.y,p.z,goal.x,goal.y,goal.z,std::sqrt(p.GetSquaredDistance(goal)));}
    if(action==control::Recovery::handoff){logger::warn("Service recovery exhausted actor={:08X} reason={}",server->GetFormID(),reason);FailApproach("call_path_failed",reason);return;}
    if(action==control::Recovery::queuedRecheck&&priority::RecheckOwnedTravel(server)){
        ++current.recoveries;current.motion.Reset(p.x,p.y,p.z);current.lost=0;
        logger::info("Service stalled Travel rechecked actor={:08X} attempt={} token={}; package and invitation retained",server->GetFormID(),current.recoveries,lease.token);
        return;
    }
    ++current.recoveries;current.phase=Phase::preparing;current.stage=0;current.resetIssued=false;current.sceneStop={};current.lost=0;current.controlWait=0;current.controlKind=-1;
    logger::info("Service automatic path recovery actor={:08X} attempt={} reason={}; no player stand required",server->GetFormID(),current.recoveries,reason);
}
void ReleaseAfterOrder(RE::Actor* server,const char* reason){
    // The actual order owns delivery. External consumers cannot delay the
    // established release-on-commit behavior by forgetting their lease.
    lease.holds=0;
    if(!current.fromCall){
        logger::info("Passive service ended actor={:08X} order={} reason={}; native AI unchanged",server->GetFormID(),current.order,reason);
        Finish(nullptr,false);return;
    }
    current.phase=Phase::releasing;current.stage=0;current.sceneStop={};
    busy=false;
    priority::Release(server,false);
    logger::info("Service order handoff actor={:08X} order={} reason={}; release independent of eating/standing",server->GetFormID(),current.order,reason);
}
bool CleanBeforeOrder(RE::Actor* server,bool matchingContext){
    current.autoClear=call::GetSettings().autoClearBeforeTalk;
    if(!AutoCleanupAllowed(CleanupEntry::orderBegin,current.phase,current.fromCall,visit,
                           current.autoClear,current.cleaned,matchingContext))return true;
    if(current.table.regions.empty())current.table=cleanup::Capture();
    // A different chair at the same table, or food restored after seat observation,
    // must also be protected. Called service clears leftovers only when ordered.
    if(!current.fromCall&&ProtectPaidMeal(current.table))return true;
    // One synchronous attempt before the stock snapshot. Never retry after the
    // order can have placed fresh food, including when no table was confirmed.
    ++current.cleanAttempts;current.cleaned=cleanup::ExecuteNow(server,false,false,&current.table);
    logger::info("Service cleanup checkpoint actor={:08X} entry=order begin before snapshot from_call={} attempt={} executed={} captured_regions={} phase={}",server->GetFormID(),current.fromCall,current.cleanAttempts,current.cleaned,current.table.regions.size(),static_cast<int>(current.phase));
    visit.Seal();
    if(!current.cleaned)RE::SendHUDMessage::ShowHUDMessage(T("call_cleanup_skipped"));
    return current.cleaned;
}
}
bool Busy(){return busy.load();}
bool CanRepeatCall(){return repeatBell.load();}
bool BackendReady(){return dataReady.load()&&legacyReady.load();}
std::uint64_t Generation(){return generation.load();}
void BackendDataLoaded(bool value){dataReady=value;legacyReady=GetModuleHandleW(L"PullUpAChairService.dll")==nullptr;logger::info("Unified service backend loaded={} legacy_migration_required={}",value,!legacyReady.load());if(value)priority::ReportCompatibility();}
bool Available(){const auto player=RE::PlayerCharacter::GetSingleton();return BackendReady()&&player&&IsSeated(player)&&FindInn(player)&&!player->IsInCombat()&&SelectServer(player,call::GetSettings().prioritizeService);}
bool CanWalkForService(RE::Actor* server){
    // Rechecked by Papyrus when a delayed movement helper actually starts.
    const auto player=RE::PlayerCharacter::GetSingleton();
    return BackendReady()&&active&&current.fromCall&&!current.prioritize&&server&&
        current.server==server->GetHandle()&&(current.phase==Phase::preparing||current.phase==Phase::approaching)&&
        Eligible(server,player)&&player&&IsSeated(player)&&!player->IsInCombat()&&player->GetParentCell()&&
        player->GetParentCell()->GetFormID()==current.cell&&player->GetOccupiedFurniture()==current.seat&&
        priority::CanPrepare(server,false);
}
bool Request(){
    if(!BackendReady())return false;
    if(active&&RepeatBellAllowed(current.fromCall,current.phase)) {
        const auto player=RE::PlayerCharacter::GetSingleton();const auto inn=FindInn(player);
        const auto ref=current.server.get();const auto server=ref?ref->As<RE::Actor>():nullptr;
        if(!player||!IsSeated(player)||player->IsInCombat()||!inn||inn->GetFormID()!=current.inn||!player->GetParentCell()||player->GetParentCell()->GetFormID()!=current.cell||player->GetOccupiedFurniture()!=current.seat||!Eligible(server,player))return false;
        recorded::OrderingBell();
        logger::info("Service bell repeated actor={:08X}; existing approach and cleanup preserved",server->GetFormID());
        return true;
    }
    if(Busy())return false;if(active)Finish();const auto player=RE::PlayerCharacter::GetSingleton();
    if(!player||!IsSeated(player)||!FindInn(player)||player->IsInCombat()||!player->GetParentCell())return false;
    const auto server=SelectServer(player,call::GetSettings().prioritizeService,{},true);if(!server){RE::SendHUDMessage::ShowHUDMessage(T("call_no_server"));return false;}
    ObserveSeat(player);
    Initialize(server,player,true);
    recorded::OrderingBell();
    RE::SendHUDMessage::ShowHUDMessage(T("call_approaching"));
    logger::info("Service call lifecycle started actor={:08X} role={} priority={} auto_clear={} distance={}",server->GetFormID(),RoleOf(server)==selection::Role::waiter?"waiter":"owner",current.prioritize,current.autoClear,std::sqrt(server->GetPosition().GetSquaredDistance(player->GetPosition())));
    Update();return true;
}
void Update(){
    if(!BackendReady())return;
    const auto ui=RE::UI::GetSingleton();if(!ui)return;
    if(ui->IsMenuOpen("Loading Menu")||ui->IsMenuOpen("Main Menu"))return;
    const auto player=RE::PlayerCharacter::GetSingleton();ObserveSeat(player);
    const bool dialogue=ui->IsMenuOpen("Dialogue Menu");
    if(!active){
        if(!dialogue||!visit.Pending()||!call::GetSettings().autoClearBeforeTalk||call::Busy()||!player||player->IsInCombat())return;
        const auto topics=RE::MenuTopicManager::GetSingleton();const auto speaker=topics?topics->speaker.get():RE::NiPointer<RE::TESObjectREFR>{};
        auto staff=speaker?speaker->As<RE::Actor>():nullptr;
        if(!topics||!topics->IsCurrentSpeaker(topics->speaker)||!Eligible(staff,player)||
           staff->GetPosition().GetSquaredDistance(player->GetPosition())>arrivalDistance*arrivalDistance)return;
        Initialize(staff,player);current.phase=Phase::talking;current.dialogueSeen=true;
        NotifyReady();
        logger::info("Passive seated staff dialogue adopted actor={:08X} seat={:08X}; no bell required, native AI unchanged",staff->GetFormID(),visit.seat);
    }
    const auto now=std::chrono::steady_clock::now();const auto dt=std::clamp(std::chrono::duration<float>(now-current.tick).count(),0.f,.5f);current.tick=now;
    const auto ref=current.server.get();const auto server=ref?ref->As<RE::Actor>():nullptr;
    if(current.phase==Phase::releasing){
        // Clear our package at commit, but never close an active player dialogue.
        if(!server){Finish(nullptr,false);return;}
        if(dialogue||ui->GameIsPaused())return;
        current.stage+=dt;
        // ReleaseAfterOrder has already surrendered our package and marker.
        // New ambient chatter after that point belongs to the normal AI.
        Finish(nullptr,false);return;
    }
    const auto inn=FindInn(player);
    const bool context=player&&IsSeated(player)&&!player->IsInCombat()&&inn&&inn->GetFormID()==current.inn&&player->GetParentCell()&&player->GetParentCell()->GetFormID()==current.cell&&player->GetOccupiedFurniture()==current.seat;
    if(!context){logger::info("Service lifecycle cancelled: seat/cell/combat changed");Finish();return;}
    if(!Eligible(server,player)){
        logger::info("Service employee unavailable; trying next candidate before dialogue");
        if(!TryNextServer("employee became unavailable"))Finish();return;
    }
    const bool blocked=ui->GameIsPaused()||call::InteractionBlocked();
    if(!ui->GameIsPaused()&&lease.Advance(dt))logger::warn("External service lease timed out token={}; service can finish",lease.token);
    if(!ui->GameIsPaused())current.stage+=dt;
    // An engine/mod force-greet can open dialogue before our arrival branch runs.
    // Verify the speaker. Conversation adoption only tracks service; cleanup
    // waits for OrderStarted even if this is an original inn force-greet.
    const auto topics=RE::MenuTopicManager::GetSingleton();
    if(dialogue&&BeforeOrder(current.phase)&&topics&&topics->IsCurrentSpeaker(current.server)&&
       server->GetPosition().GetSquaredDistance(player->GetPosition())<=arrivalDistance*arrivalDistance){
        if(current.phase!=Phase::talking){
            logger::info("Service adopted existing waiter dialogue actor={:08X} previous_phase={} cleanup={}",server->GetFormID(),static_cast<int>(current.phase),current.cleaned);
            current.phase=Phase::talking;current.dialogueSeen=true;
            NotifyReady();
        }
    }
    if(current.phase==Phase::talking||current.phase==Phase::ordering){
        // A saved legacy Papyrus stack may finish an old EvaluatePackage after
        // migration. Restore our hold while protecting actual player dialogue
        // and foreign quest control; ordinary engine NPC chatter can be ended.
        if(current.fromCall&&current.prioritize&&priority::NeedsResume(server)&&!priority::Scene(server))
            if(priority::Acquire(server,true)==priority::Result::ready)
                logger::info("Service restored own hold actor={:08X}; dialogue and order preserved",server->GetFormID());
        current.dialogueSeen=current.dialogueSeen||dialogue;
        if(current.phase==Phase::talking&&!lease.holds&&((current.dialogueSeen&&!dialogue&&!blocked)||(!current.dialogueSeen&&current.stage>2.f))){ReleaseAfterOrder(server,"dialogue closed without order");return;}
        if(current.phase==Phase::ordering&&current.stage>180.f&&!blocked){ReleaseAfterOrder(server,"ordering timeout");return;}
        return;
    }
    if(blocked)return;current.elapsed+=dt;current.callElapsed+=dt;
    if(current.callElapsed>=selection::callTimeout){Finish("call_path_failed");return;}
    if(current.elapsed>=35.f){FailApproach("call_path_failed","approach timeout");return;}
    const auto sit=server->AsActorState()->GetSitSleepState();
    if(server->IsPlayerTeammate()||server->IsHostileToActor(player)||sit==RE::SIT_SLEEP_STATE::kIsSleeping||sit==RE::SIT_SLEEP_STATE::kWantToSleep){FailApproach("call_dispatch_failed","employee unavailable or sleeping");return;}
    if(const auto scene=priority::Scene(server)){
        if(current.prioritize&&priority::AmbientScene(scene)){
            current.phase=Phase::preparing;current.resetIssued=false;
            switch(current.sceneStop.Poll(scene->GetFormID(),dt)){
                case scenes::StopStep::request:{
                    priority::Trace(server,"interruptible chatter interrupted service");
                    const bool sent=priority::StopAmbient(server);current.sceneStop.Dispatched(sent);
                    logger::info("Service scene stop attempt actor={:08X} scene={:08X} attempt={} accepted={} token={}",server->GetFormID(),scene->GetFormID(),current.sceneStop.attempts,sent,lease.token);break;
                }
                case scenes::StopStep::dispatchFailed:FailApproach("call_dispatch_failed","ambient scene stop dispatch failed after bounded retries");break;
                case scenes::StopStep::shutdownTimeout:FailApproach("call_dispatch_failed","accepted ambient scene stop did not finish shutdown");break;
                default:break;
            }
        }else WaitForControl(server,priority::Result::busy,"protected scene interrupted service",dt);
        return;
    }
    if(current.sceneStop.scene){
        logger::info("Service scene cleared actor={:08X} scene={:08X} requested={} attempts={} token={}; preparing Travel now",server->GetFormID(),current.sceneStop.scene,current.sceneStop.accepted,current.sceneStop.attempts,lease.token);
        current.sceneStop={};current.controlWait=0;current.controlKind=-1;current.lost=0;
        const auto p=server->GetPosition();current.motion.Reset(p.x,p.y,p.z);
    }
    if(current.phase==Phase::preparing){
        if(current.prioritize){
            if(!current.resetIssued){
                if(!priority::ResetBefore(server)){WaitForControl(server,priority::Result::busy,"employee control unavailable during preparation",dt);return;}
                current.resetIssued=true;
            }
            const auto result=priority::Acquire(server);
            if(result==priority::Result::busy||result==priority::Result::transition){WaitForControl(server,result,"waiting for approach control",dt);return;}
            if(result!=priority::Result::ready){FailApproach("call_dispatch_failed","approach package unavailable");return;}
        }else if(!LegacyPath(server)){FailApproach("call_dispatch_failed","legacy path dispatch failed");return;}
        current.phase=Phase::approaching;current.stage=0;current.lost=0;current.controlWait=0;current.controlKind=-1;current.sceneStop={};
        const auto p=server->GetPosition();current.motion.Reset(p.x,p.y,p.z);return;
    }
    if(current.prioritize&&priority::NeedsResume(server)){
        if(current.interruptions>=control::resumeLimit){FailApproach("call_dispatch_failed","repeated service interruptions");return;}
        const auto result=priority::Acquire(server);
        if(result==priority::Result::busy||result==priority::Result::transition){WaitForControl(server,result,"approach interrupted by another package",dt);return;}
        if(result!=priority::Result::ready){FailApproach("call_dispatch_failed","approach control could not resume");return;}
        current.controlWait=0;current.controlKind=-1;current.lost=0;
        ++current.interruptions;
        const auto p=server->GetPosition();current.motion.Reset(p.x,p.y,p.z);
        logger::info("Service approach resumed actor={:08X} token={}; same destination, no new bell",server->GetFormID(),lease.token);
    }else if(current.prioritize){current.controlWait=0;current.controlKind=-1;}
    const bool atTable=server->GetPosition().GetSquaredDistance(player->GetPosition())<=arrivalDistance*arrivalDistance;
    if(!atTable){
        const auto p=server->GetPosition();current.motion.Observe(p.x,p.y,p.z,dt);
        if(current.prioritize){
            current.lost=priority::Owns(server)?0.f:current.lost+dt;
            if(NeedsRecovery(current.motion.still,current.lost)){RecoverPath(server,current.lost>=lostPackageGrace?"service package lost":"no movement for four seconds");return;}
            if(auto scene=priority::Scene(server);scene){RecoverPath(server,"scene interrupted approach");return;}
        }else if(current.outcome->done){
            if(current.pathRetries++==0){if(!LegacyPath(server))FailApproach("call_dispatch_failed","legacy path retry dispatch failed");}else FailApproach("call_path_failed","legacy path retries exhausted");
        }
        return;
    }
    if(priority::Scene(server)){RecoverPath(server,"scene present at table");return;}
    if(dialogue)return;
    if(current.prioritize){
        const auto result=priority::Acquire(server,true);
        if(result==priority::Result::busy||result==priority::Result::transition){WaitForControl(server,result,"waiting for hold before player dialogue",dt);return;}
        if(result!=priority::Result::ready){FailApproach("call_dispatch_failed","arrival hold unavailable");return;}
    }
    current.phase=Phase::talking;current.stage=0;repeatBell=false;
    NotifyReady();
    const bool accepted=server->ActivateRef(player,0,nullptr,1,false);
    logger::info("Service arrived actor={:08X} conversation_started={} cleanup={}; lease retained until served/cancelled",server->GetFormID(),accepted,current.cleaned);
    if(!accepted){RE::SendHUDMessage::ShowHUDMessage(T("call_arrived"));ReleaseAfterOrder(server,"activation refused");}
}
void OrderStarted(RE::Actor* server,std::uint32_t token){
    if(!BackendReady())return;
    const auto player=RE::PlayerCharacter::GetSingleton();if(!token||!Eligible(server,player)||!IsSeated(player))return;
    repeatBell=false;
    ObserveSeat(player);
    if(!active){Initialize(server,player);}
    else if(current.server!=server->GetHandle()){
        const bool sameTable=player->GetParentCell()&&player->GetParentCell()->GetFormID()==current.cell&&player->GetOccupiedFurniture()==current.seat&&FindInn(player)&&FindInn(player)->GetFormID()==current.inn;
        if(BeforeOrder(current.phase)&&sameTable){
            auto old=current.server.get();if(current.fromCall)priority::Release(old?old->As<RE::Actor>():nullptr);
            logger::info("Call ordering switched staff previous={:08X} actual={:08X}; captured table and pending cleanup retained",old?old->GetFormID():0,server->GetFormID());
            current.server=server->GetHandle();
        }else{Finish(nullptr,false);Initialize(server,player);}
    }
    if(current.fromCall&&current.prioritize)priority::ReserveForService(server);
    const bool context=!player->IsInCombat()&&player->GetParentCell()&&player->GetParentCell()->GetFormID()==current.cell&&player->GetOccupiedFurniture()==current.seat&&FindInn(player)&&FindInn(player)->GetFormID()==current.inn;
    if(context&&BeforeOrder(current.phase)){
        CleanBeforeOrder(server,context);
    }
    logger::info("Ordering entry cleanup audit actor={:08X} order={} from_call={} enabled={} cleaned={} attempts={} same_context={} visit_sealed={}",server->GetFormID(),token,current.fromCall,call::GetSettings().autoClearBeforeTalk,current.cleaned,current.cleanAttempts,context,visit.sealed);
    // Never allow a later passive greeting/reopened menu to clear this order.
    visit.Seal();current.phase=Phase::ordering;current.order=token;current.stage=0;
    NotifyReady();
    logger::info("Service tracks ordering actor={:08X} order={}",server->GetFormID(),token);
}
void OrderFinished(RE::Actor* server,std::uint32_t token,bool committed){
    if(active&&server&&current.server==server->GetHandle()&&MatchesOrder(current.order,token))ReleaseAfterOrder(server,committed?"all food committed on table":"order cancelled/failed");
}
void Reset(){InvalidateBackend();if(active)Finish();active=false;busy=false;repeatBell=false;current={};visit={};visitTable={};recoverActors.clear();}
void PrepareLoad(){InvalidateBackend();if(current.fromCall)if(auto ref=current.server.get())priority::Release(ref->As<RE::Actor>(),false);active=false;busy=false;repeatBell=false;current={};visit={};visitTable={};recoverActors.clear();}
void Save(SKSE::SerializationInterface* io){if(active&&current.fromCall)if(auto server=current.server.get()){auto id=server->GetFormID();io->WriteRecord(recordType,1,id);}}
void Load(SKSE::SerializationInterface* io,std::uint32_t version,std::uint32_t length){
    if(version!=1||length!=sizeof(RE::FormID))return;RE::FormID id{},resolved{};
    if(io->ReadRecordData(id)==sizeof(id)&&io->ResolveFormID(id,resolved)&&recoverActors.size()<8)recoverActors.push_back(resolved);
}
void Recover(){for(auto id:recoverActors){const auto actor=RE::TESForm::LookupByID<RE::Actor>(id);priority::Release(actor);}recoverActors.clear();}
namespace {
bool ResolveAPI(std::uint32_t server,api::Supply* out){
    if(!out||out->size!=sizeof(*out))return false;*out={};if(!dataReady)return false;
    auto actor=RE::TESForm::LookupByID<RE::Actor>(server);if(!actor)return false;
    const auto supplier=suppliers::Resolve(actor);const auto inn=FindInn(actor);
    out->inn=inn?inn->GetFormID():0;out->merchant=supplier.id.merchant;
    out->chest=supplier.id.stock;out->faction=supplier.id.faction;
    const auto player=RE::PlayerCharacter::GetSingleton();
    if(player&&FindInn(player)==inn&&player->GetParentCell())player->GetParentCell()->ForEachReference([&](RE::TESObjectREFR* ref){
        if(Eligible(ref?ref->As<RE::Actor>():nullptr,player))++out->staffCount;return RE::BSContainer::ForEachResult::kContinue;
    });
    if(out->chest)out->status=static_cast<unsigned>(out->merchant?api::Status::ready:api::Status::stockOnly);
    return out->status!=0;
}
std::uint32_t InventoryAPI(std::uint32_t server,api::StockItem* out,std::uint32_t capacity){
    api::Supply supply;if(!ResolveAPI(server,&supply))return 0;
    const auto chest=RE::TESForm::LookupByID<RE::TESObjectREFR>(supply.chest);if(!chest)return 0;
    std::vector<api::StockItem> stock;
    for(const auto& [form,data]:chest->GetInventory())if(form&&data.first>0)stock.push_back({form->GetFormID(),static_cast<unsigned>(data.first)});
    std::sort(stock.begin(),stock.end(),[](auto a,auto b){return a.form<b.form;});
    if(out)std::copy_n(stock.begin(),std::min<std::size_t>(stock.size(),capacity),out);
    return static_cast<unsigned>(stock.size());
}
std::uint32_t RequestAPI(){return call::RequestExternal()?lease.token:0;}
bool StateAPI(api::Service* out){if(!out||out->size!=sizeof(*out))return false;*out=SnapshotService();return true;}
bool RetainAPI(std::uint32_t token){return BackendReady()&&active&&current.phase!=Phase::releasing&&lease.Retain(token);}
bool ReleaseAPI(std::uint32_t token){return BackendReady()&&active&&current.phase!=Phase::releasing&&lease.Release(token);}
void CancelAPI(){
    // API cancellation cannot tear down an order currently committing food.
    if(!BackendReady()||!active||current.phase==Phase::ordering)return;
    Finish();
}
void LegacyReadyAPI(bool value){legacyReady=value;logger::info("Unified service legacy migration ready={}",value);}
const innservice::backend::API backend{
    sizeof(innservice::backend::API),innservice::backend::version,
    {sizeof(api::API),api::version,ResolveAPI,InventoryAPI,RequestAPI,StateAPI,RetainAPI,ReleaseAPI,CancelAPI},
    LegacyReadyAPI,BackendReady
};
}
const innservice::backend::API* GetBackend(std::uint32_t requested){return requested==innservice::backend::version?&backend:nullptr;}
}
extern "C" __declspec(dllexport) const innservice::backend::API* PullUpAChairOrdering_GetServiceBackend(std::uint32_t requested){return inn::service::GetBackend(requested);}
