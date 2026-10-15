#include "PCH.h"
#include "ServiceManager.h"
#include "ServicePolicy.h"
#include "MigrationPolicy.h"
#include "Supply.h"
#include "Settings.h"
#include "RecordedSounds.h"
#include "InnServiceBackend.h"
#include <cstring>
#include <limits>
namespace innservice {
namespace {
using Clock=std::chrono::steady_clock;
struct Call {
    api::Service info;RE::ObjectRefHandle actor,seat;
    RE::FormID cell{};float elapsed{};unsigned holds{};
    bool talkAttempted{},hadConversation{},clearSent{};
    Clock::time_point last=Clock::now();
};
std::recursive_mutex mutex;
Call call;View view;
std::uint32_t sequence=(GetTickCount()&0x1FFFFFFF)+1;
std::atomic_bool working{},queued{};
std::atomic_uint epoch{1};
std::condition_variable_any wake;std::mutex waitMutex;
std::jthread worker;
enum class BackendMode { unavailable,standalone,integrated };
enum class Migration { pending,ready,failed };
std::atomic<BackendMode> mode{BackendMode::unavailable};
std::atomic<Migration> migration{Migration::pending};
std::atomic<const backend::API*> engine{};
bool backendDiscovered{};
bool listenerRegistered{};
std::atomic_bool sessionReady{};
MigrationProgress migrationProgress;
struct DeferredRequest {
    bool pending{};RE::ObjectRefHandle seat;RE::FormID cell{};
    Clock::time_point deadline{};
} deferred;
bool MenusBlocked() {
    auto ui=RE::UI::GetSingleton();
    return !ui||ui->GameIsPaused()||BlockingMenus()||ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME)||
        ui->IsMenuOpen("Loading Menu")||ui->IsMenuOpen("Main Menu");
}
RE::TESQuest* Quest(){return RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESQuest>(0x800,"Pull up a chair Service.esp");}
RE::TESGlobal* Flag(){return RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESGlobal>(0x802,"Pull up a chair Service.esp");}
void SetFlag(float value){if(auto flag=Flag())flag->value=value;}
void Status(const char* key,bool notify=false){view.status=key;if(notify)RE::SendHUDMessage::ShowHUDMessage(Text(key));}
void Changed(api::Phase phase){call.info.phase=static_cast<std::uint32_t>(phase);view.service=call.info;call.elapsed=0;}
void Message(std::uint32_t type){
    auto payload=call.info;const auto generation=epoch.load();
    // Consumers may call our API synchronously. Never dispatch while holding mutex.
    SKSE::GetTaskInterface()->AddTask([payload,type,generation]()mutable{
        if(generation==epoch.load())SKSE::GetMessagingInterface()->Dispatch(type,&payload,sizeof(payload),nullptr);
    });
}
bool DispatchClear() {
    auto quest=Quest();auto vm=RE::BSScript::Internal::VirtualMachine::GetSingleton();if(!quest||!vm)return false;
    auto policy=vm->GetObjectHandlePolicy();if(!policy)return false;
    const auto handle=policy->GetHandleForObject(static_cast<RE::VMTypeID>(quest->GetFormType()),quest);
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
    return vm->DispatchMethodCall(handle,"PullUpAChairServiceControl","ClearServiceAlias",RE::MakeFunctionArguments(static_cast<int>(call.info.token)),callback);
}
void Cancel(const char* reason,bool notify=false) {
    if(call.info.phase==static_cast<std::uint32_t>(api::Phase::idle)){Status(reason,notify);return;}
    if(call.info.phase==static_cast<std::uint32_t>(api::Phase::releasing))return;
    SetFlag(0);Changed(api::Phase::releasing);Status(reason,notify);call.clearSent=DispatchClear();
    // The package condition is already false even if the VM is temporarily busy.
    if(auto actor=call.actor.get())if(auto npc=actor->As<RE::Actor>())npc->EvaluatePackage();
    logger::info("Service ending token={} reason={} clearQueued={}",call.info.token,reason,call.clearSent);
}
void Refresh(RE::TESObjectREFR* ref) {
    if(mode!=BackendMode::standalone) {
        View next;
        next.status=BackendStatus();
        if(const auto core=engine.load()) {
            core->service.state(&next.service);
            const auto id=ref?ref->GetFormID():0;
            core->service.resolve(id,&next.supply);
            auto location=RE::TESForm::LookupByID<RE::BGSLocation>(next.supply.inn);
            auto owner=RE::TESForm::LookupByID<RE::Actor>(next.supply.merchant);
            auto staff=RE::TESForm::LookupByID<RE::Actor>(next.service.server);
            next.innName=location?location->GetName():"";
            next.merchantName=owner?owner->GetName():"";
            next.serverName=staff?staff->GetName():"";
            const auto count=core->service.inventory(id,nullptr,0);
            // Bound UI allocation even if a third-party backend violates the contract.
            std::vector<api::StockItem> stock(std::min<std::uint32_t>(count,65536));
            if(!stock.empty()) {
                const auto actual=core->service.inventory(id,stock.data(),static_cast<std::uint32_t>(stock.size()));
                stock.resize(std::min<std::size_t>(stock.size(),actual));
                for(const auto& item:stock)if(auto form=RE::TESForm::LookupByID(item.form);form&&item.count)
                    next.stock.emplace_back(form->GetName(),static_cast<int>(std::min<std::uint32_t>(item.count,std::numeric_limits<int>::max())));
            }
            std::sort(next.stock.begin(),next.stock.end());
        }
        std::lock_guard lock(mutex);view=std::move(next);return;
    }
    std::lock_guard lock(mutex);
    const auto result=Resolve(ref);view.supply=result.supply;view.stock.clear();
    auto location=FindInn(ref);view.innName=location?location->GetName():"";
    auto owner=RE::TESForm::LookupByID<RE::Actor>(result.supply.merchant);view.merchantName=owner?owner->GetName():"";
    auto chest=RE::TESForm::LookupByID<RE::TESObjectREFR>(result.supply.chest);
    if(chest)for(const auto& [object,data]:chest->GetInventory())if(object&&data.first>0)view.stock.emplace_back(object->GetName(),data.first);
    std::sort(view.stock.begin(),view.stock.end());
}
bool ContextValid(RE::PlayerCharacter* player,RE::Actor* actor) {
    const auto inn=FindInn(player);const auto seat=player?player->GetOccupiedFurniture():RE::ObjectRefHandle{};
    return Continues({Seated(player),seat==call.seat,player&&player->GetParentCell()&&player->GetParentCell()->GetFormID()==call.cell,
        inn&&inn->GetFormID()==call.info.inn,player&&player->IsInCombat(),actor&&!actor->IsDead()&&!actor->IsDisabled()&&
        !actor->IsInCombat()&&actor->GetParentCell()==player->GetParentCell()&&FindInn(actor)==inn});
}
}
bool UsesCore(){return mode!=BackendMode::standalone;}
const char* BackendStatus(){
    if(mode==BackendMode::standalone)return "idle";
    if(mode!=BackendMode::integrated)return "core_unavailable";
    if(migration==Migration::pending)return "core_migration";
    if(migration==Migration::failed)return "core_migration_failed";
    return "core_integrated";
}
void DiscoverBackend(){
    if(backendDiscovered){
        if(mode==BackendMode::integrated&&!listenerRegistered)
            listenerRegistered=SKSE::GetMessagingInterface()->RegisterListener("PullUpAChairOrdering",HandleCoreMessage);
        return;
    }
    backendDiscovered=true;
    auto module=GetModuleHandleW(backend::module);
    if(!module){
        // A present but failed/unloaded ordering plugin must not silently start a second engine.
        std::error_code error;
        const bool installed=std::filesystem::exists("Data/SKSE/Plugins/PullUpAChairOrdering.dll",error);
        engine=nullptr;
        if(backend::SelectRoute(installed||bool(error),false)!=backend::Route::standalone){mode=BackendMode::unavailable;logger::error("Ordering DLL exists or its presence cannot be checked, but it is not loaded; standalone fallback disabled");}
        else {mode=BackendMode::standalone;logger::info("Ordering DLL absent; standalone service fallback selected");}
        return;
    }
    auto factory=reinterpret_cast<backend::GetAPI>(GetProcAddress(module,backend::factory));
    const auto core=factory?factory(backend::version):nullptr;
    if(backend::SelectRoute(true,backend::Compatible(core))!=backend::Route::core){
        engine=nullptr;mode=BackendMode::unavailable;
        logger::error("Ordering DLL present but service backend v1 incompatible; standalone AI and input disabled");return;
    }
    engine=core;mode=BackendMode::integrated;
    if(!listenerRegistered)listenerRegistered=SKSE::GetMessagingInterface()->RegisterListener("PullUpAChairOrdering",HandleCoreMessage);
    logger::info("Ordering service backend v1 attached; shared AI/input/supply, legacy migration gate retained; listener={}",listenerRegistered);
}
void HandleCoreMessage(SKSE::MessagingInterface::Message* message){
    if(message&&mode==BackendMode::integrated&&message->sender&&std::strcmp(message->sender,"PullUpAChairOrdering")==0&&
       message->type==api::languageChanged&&message->data&&message->dataLen==sizeof(std::uint32_t)) {
        std::uint32_t language{};std::memcpy(&language,message->data,sizeof(language));
        if(language<=1)ApplyMenuLanguage(language==1);
        return;
    }
    if(!message||mode!=BackendMode::integrated||!message->sender||std::strcmp(message->sender,"PullUpAChairOrdering")!=0||
       (message->type!=api::serviceReady&&message->type!=api::serviceEnded)||!message->data||message->dataLen!=sizeof(api::Service))return;
    api::Service payload;std::memcpy(&payload,message->data,sizeof(payload));
    if(payload.size!=sizeof(payload))return;
    const auto type=message->type,generation=epoch.load();
    SKSE::GetTaskInterface()->AddTask([payload,type,generation]()mutable{
        if(generation!=epoch.load()||mode!=BackendMode::integrated)return;
        if(type==api::serviceReady){
            api::Service current;const auto core=engine.load();
            if(!core||!core->service.state(&current)||current.token!=payload.token||current.phase==0||current.phase==5)return;
        }
        // This dispatch is deliberately outside every bridge lock and preserves sender.
        SKSE::GetMessagingInterface()->Dispatch(type,&payload,sizeof(payload),nullptr);
        QueueRefresh();
    });
}
std::uint32_t Request() {
    if(UsesCore()) {
        const auto core=engine.load();
        if(!core){std::lock_guard lock(mutex);Status("core_unavailable",true);return 0;}
        if(migration==Migration::failed){Recover();RE::SendHUDMessage::ShowHUDMessage(Text(BackendStatus()));return 0;}
        if(migration!=Migration::ready||!core->ready()){
            std::lock_guard lock(mutex);Status(migration==Migration::ready?"core_waiting":BackendStatus(),true);return 0;
        }
        const auto token=core->service.request();
        Refresh(RE::PlayerCharacter::GetSingleton());return token;
    }
    std::lock_guard lock(mutex);
    auto player=RE::PlayerCharacter::GetSingleton();
    if(!Seated(player)){Status("sit_first",true);return 0;}
    auto inn=FindInn(player);if(!inn){Status("not_inn",true);return 0;}
    if(player->IsInCombat()){Status("combat",true);return 0;}
    if(MenusBlocked()){Status("dialogue_busy",true);return 0;}
    if(call.info.phase) {
        auto ref=call.actor.get();auto actor=ref?ref->As<RE::Actor>():nullptr;
        if(RepeatBellAllowed(call.info.phase)&&ContextValid(player,actor)) {
            inn::recorded::IndependentBell();
            logger::info("Service bell repeated token={} server={:08X}; existing request unchanged",call.info.token,call.info.server);
            return call.info.token;
        }
        Status("busy",true);return 0;
    }
    auto quest=Quest();if(!quest||!Flag()){Status("plugin_missing",true);return 0;}
    const auto resolved=Resolve(player);Refresh(player);
    const auto actorID=ChooseServer(resolved.staff,inn->GetFormID(),player->GetParentCell()->GetFormID(),settings.radius);
    auto actor=RE::TESForm::LookupByID<RE::Actor>(actorID);
    if(!actor){Status("no_staff",true);return 0;}
    call={};call.info.token=++sequence;call.info.server=actorID;call.info.inn=inn->GetFormID();
    call.info.merchant=resolved.supply.merchant;call.info.chest=resolved.supply.chest;
    call.actor=actor->GetHandle();call.seat=player->GetOccupiedFurniture();call.info.seat=call.seat.get()->GetFormID();
    call.cell=player->GetParentCell()->GetFormID();view.serverName=actor->GetName();
    Changed(api::Phase::starting);Status("calling",true);
    quest->Start();working=true;wake.notify_all();
    inn::recorded::IndependentBell();
    logger::info("Service requested token={} inn={:08X} server={:08X} merchant={:08X} chest={:08X}",call.info.token,call.info.inn,actorID,call.info.merchant,call.info.chest);
    return call.info.token;
}
void QueueRequest(bool waitForMenu){const auto generation=epoch.load();SKSE::GetTaskInterface()->AddTask([generation,waitForMenu]{
    if(generation!=epoch.load())return;
    if(waitForMenu&&MenusBlocked()){
        auto player=RE::PlayerCharacter::GetSingleton();
        if(!Seated(player)||!player->GetParentCell()){std::lock_guard lock(mutex);Status("sit_first",true);return;}
        std::lock_guard lock(mutex);
        deferred={true,player->GetOccupiedFurniture(),player->GetParentCell()->GetFormID(),Clock::now()+10s};
        Status("core_menu_pending");working=true;wake.notify_all();return;
    }
    Request();
});}
void QueueCancel(){const auto generation=epoch.load();SKSE::GetTaskInterface()->AddTask([generation]{
    if(generation!=epoch.load())return;
    {std::lock_guard lock(mutex);deferred={};}
    if(UsesCore()){if(const auto core=engine.load();core&&migration==Migration::ready)core->service.cancel();QueueRefresh();return;}
    std::lock_guard lock(mutex);Cancel("cancelled",true);
});}
void QueueRefresh(){const auto generation=epoch.load();SKSE::GetTaskInterface()->AddTask([generation]{
    if(generation!=epoch.load())return;
    if(UsesCore()&&migration==Migration::failed)Recover();
    else Refresh(RE::PlayerCharacter::GetSingleton());
});}
void QueueRecover(){const auto generation=epoch.load();SKSE::GetTaskInterface()->AddTask([generation]{if(generation==epoch.load())Recover();});}
void SessionLoaded(){sessionReady=true;QueueRecover();}
void Tick() {
    std::unique_lock lock(mutex);
    if(deferred.pending){
        const auto player=RE::PlayerCharacter::GetSingleton();
        const bool same=Seated(player)&&player->GetOccupiedFurniture()==deferred.seat&&player->GetParentCell()&&
            player->GetParentCell()->GetFormID()==deferred.cell&&!player->IsInCombat();
        if(!same||Clock::now()>=deferred.deadline){deferred={};Status(same?"core_menu_expired":"context_changed",true);}
        else if(!MenusBlocked()){
            deferred={};lock.unlock();Request();lock.lock();
        }
    }
    if(UsesCore()){
        // The legacy call is a migration ticket only; never advance its AI phases.
        if(mode==BackendMode::integrated&&migration==Migration::pending&&!call.info.phase){
            Recover();
        }
        if(mode==BackendMode::integrated&&migration==Migration::pending&&call.info.phase==5){
            const auto now=Clock::now();
            const auto delta=std::chrono::duration<float>(now-call.last).count();call.last=now;
            const bool blocked=MenusBlocked();
            if(sessionReady&&!blocked&&!call.clearSent)call.clearSent=DispatchClear();
            if(migrationProgress.Advance(delta,sessionReady,blocked)){
                logger::warn("Legacy service migration slow token={} clearQueued={}; continuing the same ticket automatically; paused time excluded",call.info.token,call.clearSent);
            }
        }
        working=deferred.pending||(sessionReady&&mode==BackendMode::integrated&&migration==Migration::pending);
        return;
    }
    if(!call.info.phase){working=deferred.pending;return;}
    const auto now=Clock::now();const float delta=std::chrono::duration<float>(now-call.last).count();call.last=now;
    auto ui=RE::UI::GetSingleton();if((ui&&ui->GameIsPaused())||BlockingMenus())return;
    call.elapsed+=std::clamp(delta,0.f,1.f);
    if(call.info.phase==static_cast<std::uint32_t>(api::Phase::releasing)) {
        if(!call.clearSent)call.clearSent=DispatchClear();
        if(Expired(call.elapsed,20)) {
            if(auto quest=Quest())quest->Stop();
            logger::error("Service alias clear did not acknowledge token={}; package disabled",call.info.token);
            call={};view.service={};working=false;
        }
        return;
    }
    auto player=RE::PlayerCharacter::GetSingleton();auto handle=call.actor.get();auto actor=handle?handle->As<RE::Actor>():nullptr;
    if(!player||!ContextValid(player,actor)){Cancel("context_changed",true);return;}
    if(call.info.phase==static_cast<std::uint32_t>(api::Phase::starting)) {
        auto quest=Quest();if(!quest||Expired(call.elapsed,8)){Cancel("start_failed",true);return;}
        if(!quest->IsEnabled()||quest->IsStarting())return;
        quest->ForceRefIntoAlias(0,actor);
        if(quest->GetAliasedRef(0)!=call.actor){Cancel("start_failed",true);return;}
        SetFlag(1);actor->EvaluatePackage();Changed(api::Phase::approaching);return;
    }
    const bool dialogue=ui&&ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME);
    if(dialogue)if(auto topics=RE::MenuTopicManager::GetSingleton();topics&&topics->speaker&&topics->speaker!=call.actor) {
        Cancel("dialogue_busy");return;
    }
    if(dialogue){call.hadConversation=true;if(call.info.phase!=static_cast<std::uint32_t>(api::Phase::conversation))Changed(api::Phase::conversation);}
    if(call.info.phase==static_cast<std::uint32_t>(api::Phase::approaching)) {
        if(!Available(actor,player)){Cancel("staff_busy",true);return;}
        const auto a=actor->GetPosition(),p=player->GetPosition();
        if(Arrived(std::hypot(a.x-p.x,a.y-p.y),std::abs(a.z-p.z),settings.arrival)) {
            Changed(api::Phase::waiting);Status("arrived",true);Message(api::serviceReady);
        }else if(Expired(call.elapsed,settings.approachTimeout))Cancel("path_failed",true);
    }
    if(call.info.phase==static_cast<std::uint32_t>(api::Phase::waiting)) {
        if(settings.autoTalk&&!call.talkAttempted&&!call.hadConversation&&!BlockingMenus()) {
            call.talkAttempted=true;
            if(!actor->ActivateRef(player,0,nullptr,1,false))Status("talk_manually",true);
        }
        if(!call.holds&&Expired(call.elapsed,settings.readyTimeout))Cancel("finished");
        if(call.holds&&Expired(call.elapsed,settings.conversationTimeout))Cancel("finished");
    }else if(call.info.phase==static_cast<std::uint32_t>(api::Phase::conversation)) {
        if(!dialogue){Changed(api::Phase::waiting);Status("waiting");}
        else if(Expired(call.elapsed,settings.conversationTimeout))Cancel("finished");
    }
}
bool Retain(std::uint32_t token){
    if(UsesCore()){const auto core=engine.load();return core&&migration==Migration::ready&&core->service.retain(token);}
    std::lock_guard lock(mutex);if(call.info.token!=token||!call.info.phase||call.info.phase==5||call.holds>=16)return false;++call.holds;call.elapsed=0;return true;
}
bool Release(std::uint32_t token){
    if(UsesCore()){const auto core=engine.load();return core&&migration==Migration::ready&&core->service.release(token);}
    std::lock_guard lock(mutex);if(call.info.token!=token||!call.holds)return false;--call.holds;call.elapsed=0;return true;
}
bool IsReleaseCurrent(int token){
    std::lock_guard lock(mutex);
    return (!UsesCore()||migration!=Migration::ready)&&call.info.token==static_cast<std::uint32_t>(token)&&call.info.phase==5;
}
void ReleaseComplete(int token) {
    const auto generation=epoch.load();
    SKSE::GetTaskInterface()->AddTask([token,generation]{
        if(generation!=epoch.load())return;
        std::unique_lock lock(mutex);if(!IsReleaseCurrent(token))return;
        const auto quest=Quest();
        if(!quest||quest->GetAliasedRef(0)){
            call.clearSent=false;logger::warn("Legacy release callback token={} rejected: alias not empty",token);return;
        }
        // ClearServiceAlias calls this only after Clear and EvaluatePackage return.
        // The empty alias check and current epoch/token prevent stale callbacks.
        const auto core=engine.load();
        if(UsesCore()){
            if(!core||mode!=BackendMode::integrated)return;
            call={};migration=Migration::ready;working=deferred.pending;Status("core_integrated");
            logger::info("Legacy service alias migration acknowledged token={}; enabling ordering backend",token);
            lock.unlock();core->legacyReady(true);Refresh(RE::PlayerCharacter::GetSingleton());
        }else{
            Message(api::serviceEnded);logger::info("Service released token={}",token);
            call={};view.service={};working=deferred.pending;
        }
    });
}
void Recover() {
    const auto core=engine.load();
    auto player=RE::PlayerCharacter::GetSingleton();
    if(!sessionReady||(UsesCore()&&!CanMigrate(sessionReady,player&&player->GetParentCell(),MenusBlocked()))){
        working=sessionReady&&(UsesCore()&&mode==BackendMode::integrated);wake.notify_all();return;
    }
    if(UsesCore()){
        if(core)core->legacyReady(false);
        std::unique_lock lock(mutex);auto quest=Quest();SetFlag(0);
        if(!core||mode!=BackendMode::integrated){Status("core_unavailable");working=deferred.pending;return;}
        if(!quest||!Flag()){
            migration=Migration::failed;Status("core_migration_failed");working=deferred.pending;
            logger::error("Legacy migration cannot locate service quest/global; ordering remains gated");return;
        }
        if(call.info.phase==5&&migration!=Migration::ready){
            migration=Migration::pending;call.elapsed=0;call.last=Clock::now();
            if(!call.clearSent)call.clearSent=DispatchClear();
            Status("core_migration");working=true;wake.notify_all();
            logger::info("Continuing legacy migration token={} clearQueued={}; accepted VM ticket is not replaced",call.info.token,call.clearSent);return;
        }
        call={};migrationProgress={};migration=Migration::pending;
        const auto actor=quest->GetAliasedRef(0);
        // Even an empty alias is acknowledged by the legacy script. A prior timed-out
        // stack may already have cleared it while its EvaluatePackage was still running.
        call.actor=actor;call.info.token=++sequence;Changed(api::Phase::releasing);
        Status("core_migration");call.clearSent=DispatchClear();working=true;wake.notify_all();
        logger::info("Migrating legacy service alias token={} actor={:08X} clearQueued={}; ordering gate closed",call.info.token,actor.get()?actor.get()->GetFormID():0,call.clearSent);
        return;
    }
    std::lock_guard lock(mutex);auto quest=Quest();SetFlag(0);
    auto actor=quest?quest->GetAliasedRef(0):RE::ObjectRefHandle{};
    if(actor) {
        call={};call.actor=actor;call.info.token=++sequence;Changed(api::Phase::releasing);
        call.clearSent=DispatchClear();working=true;wake.notify_all();
    }
    Refresh(RE::PlayerCharacter::GetSingleton());
}
void Revert(){
    // A load invalidates queued UI, worker, relay and VM completion tasks.
    {std::lock_guard lock(mutex);++epoch;++sequence;sessionReady=false;migrationProgress={};call={};view={};deferred={};migration=Migration::pending;working=false;queued=false;}
    if(const auto core=engine.load())core->legacyReady(false);
}
View CurrentView(){std::lock_guard lock(mutex);return view;}
bool CopyState(api::Service* out){
    if(!out||out->size!=sizeof(*out))return false;
    if(UsesCore()){if(const auto core=engine.load())return core->service.state(out);*out={};return false;}
    std::lock_guard lock(mutex);*out=call.info;return true;
}
bool ResolveAPI(std::uint32_t server,api::Supply* out) {
    if(!out||out->size!=sizeof(*out))return false;
    if(UsesCore()){if(const auto core=engine.load())return core->service.resolve(server,out);*out={};return false;}
    auto ref=RE::TESForm::LookupByID<RE::TESObjectREFR>(server);*out=Resolve(ref).supply;
    return out->status==1||out->status==2;
}
std::uint32_t InventoryAPI(std::uint32_t server,api::StockItem* out,std::uint32_t capacity) {
    if(UsesCore()){const auto core=engine.load();return core?core->service.inventory(server,out,capacity):0;}
    api::Supply supply;if(!ResolveAPI(server,&supply))return 0;
    auto chest=RE::TESForm::LookupByID<RE::TESObjectREFR>(supply.chest);if(!chest)return 0;
    std::vector<api::StockItem> stock;
    for(const auto& [form,data]:chest->GetInventory())if(form&&data.first>0)stock.push_back({form->GetFormID(),static_cast<std::uint32_t>(data.first)});
    std::sort(stock.begin(),stock.end(),[](auto a,auto b){return a.form<b.form;});
    if(out)std::copy_n(stock.begin(),std::min<std::size_t>(stock.size(),capacity),out);
    return static_cast<std::uint32_t>(stock.size());
}
void Save(SKSE::SerializationInterface* io){std::lock_guard lock(mutex);io->WriteRecord(0x494E5351,1,sequence);}
void Load(SKSE::SerializationInterface* io) {
    Revert();std::uint32_t type,version,length;
    while(io->GetNextRecordInfo(type,version,length))if(type==0x494E5351&&version==1&&length==4){std::uint32_t saved{};if(io->ReadRecordData(saved)==4)sequence=std::max(sequence,saved);}
}
void InitWorker() {
    if(worker.joinable())return;
    worker=std::jthread([](std::stop_token stop){
        std::unique_lock lock(waitMutex);
        while(!stop.stop_requested()) {
            wake.wait(lock,stop,[]{return working.load();});if(stop.stop_requested())break;
            if(!queued.exchange(true)){
                const auto generation=epoch.load();
                SKSE::GetTaskInterface()->AddTask([generation]{if(generation==epoch.load()){Tick();queued=false;}});
            }
            wake.wait_for(lock,stop,250ms,[]{return !working.load();});
        }
    });
}
}
