#include "PCH.h"
#include "CallService.h"
#include "SettingsStore.h"
#include "CallFocus.h"
#include "CallFocusPolicy.h"
#include "InnServices.h"
#include "TopicVisibility.h"
#include "TableDiscovery.h"
#include "Localization.h"
#include "ServiceApproach.h"
#include "SkyPromptCall.h"
#include "TopicVisibility.h"
#include "SKSEMenuFramework.h"
#include <SKSE/InputMap.h>
namespace inn::call {
namespace {

std::mutex settingsMutex;
Settings settings;
std::atomic_uint pending{};std::atomic_bool loaded{};
std::atomic_uint64_t generation{};
std::array<std::atomic_bool,282> claimed{};
std::mutex modifierMutex;
ModifierState modifier;
bool (*orderGuard)(){};
RE::TESGlobal* dakState{};
RE::TESGlobal* dakKey{};
SKSEMenuFramework::Model::InputEvent* inputRegistration{};
SKSEMenuFramework::Model::WindowInterface* settingsWindow{};
SKSEMenuFramework::Model::Event* updateRegistration{};
std::atomic_bool updateQueued{};
std::chrono::steady_clock::time_point lastUpdate{};
std::chrono::steady_clock::time_point lastServiceUpdate{};
bool RequestsBlocked(){return !service::BackendReady()||(service::Busy()&&!service::CanRepeatCall())||(orderGuard&&orderGuard());}
bool MenusOpen() {
    const auto ui=RE::UI::GetSingleton();
    if(!ui||ui->GameIsPaused()||(settingsWindow&&settingsWindow->IsOpen)||SKSEMenuFramework::IsAnyBlockingWindowOpened())return true;
    for(auto name:{"Dialogue Menu","Console","Loading Menu","Main Menu","InventoryMenu","MagicMenu","ContainerMenu","BarterMenu","Journal Menu","MapMenu","TweenMenu","Sleep/Wait Menu","MessageBoxMenu","Book Menu","Crafting Menu"})
        if(ui->IsMenuOpen(name))return true;
    const auto control=RE::ControlMap::GetSingleton();
    return control&&(control->GetRuntimeData().textEntryCount>0||!control->IsActivateControlsEnabled());
}
struct Selection {
    TargetContext context;
    RE::FormID reference{},base{};
    int type=-1;
    bool cameraTable{};
};
bool DeviceKeyHeld(std::uint32_t key) {
    DWORD process{};GetWindowThreadProcessId(GetForegroundWindow(),&process);
    if(process!=GetCurrentProcessId())return false;
    auto input=RE::BSInputDeviceManager::GetSingleton();if(!input)return false;
    if(key>0&&key<256){
        auto device=input->GetKeyboard();
        // Same bounded curState read as CommonLib's IsPressed helper, without
        // pulling its keyboard constructor/vtable object from the prebuilt lib.
        return device&&device->IsEnabled()&&(device->GetRuntimeData().curState[key]&0x80)!=0;
    }
    if(key>=256&&key<266){auto device=input->GetMouse();return device&&device->IsEnabled()&&device->IsPressed(key-256);}
    if(key>=266&&key<282){
        auto device=input->GetGamepad();
        return device&&device->IsEnabled()&&RE::ControlMap::GetSingleton()&&device->IsPressed(SKSE::InputMap::GamepadKeycodeToMask(key));
    }
    return false;
}
void RefreshModifier() {
    // Read device state on the game thread, including while ordering/dialogue
    // blocks our HUD. Input callbacks still provide immediate press handling.
    const auto configured=FrameworkKey();
    const auto key=configured>0&&configured<282?static_cast<std::uint32_t>(configured):0;
    const bool pressed=DeviceKeyHeld(key);
    std::lock_guard lock(modifierMutex);
    const bool wasHeld=modifier.Active(dakState&&dakState->value>0);
    modifier.Synchronize(key,pressed);
    if(wasHeld&&!pressed)logger::info("DAK modifier reconciled released key={}; stale callback/script hold discarded",key);
}
Selection CallSelection(RE::PlayerCharacter* player,bool tableFocus) {
    // BTPS writes its selected reference to this same engine handle. Do not
    // cast a separate camera ray or force the user back into first person.
    auto pick=RE::CrosshairPickData::GetSingleton();auto target=pick?pick->GetActiveTarget().get():RE::NiPointer<RE::TESObjectREFR>{};
    bool cameraTable=false;
    if(tableFocus)if(auto focused=focus::Target(player)){target=focused;cameraTable=true;}
    if(!target)return {};
    const auto base=target->GetBaseObject();
    const auto type=base?base->GetFormType():RE::FormType::None;
    return {{true,target->GetParentCell()==player->GetParentCell(),target->GetHandle()==player->GetOccupiedFurniture(),
             base&&discovery::Index(base)>=0,type==RE::FormType::Static||type==RE::FormType::MovableStatic},
            target->GetFormID(),base?base->GetFormID():0,static_cast<int>(type),cameraTable};
}
void Dispatch(std::uint64_t id,RE::ObjectRefHandle seat,RE::FormID cell,Mode source) {
    if(generation.load()!=id)return;
    auto player=RE::PlayerCharacter::GetSingleton();
    if(!player||GetSettings().mode!=source||!IsSeated(player)||!FindInn(player)||player->GetOccupiedFurniture()!=seat||!player->GetParentCell()||player->GetParentCell()->GetFormID()!=cell||player->IsInCombat()||MenusOpen()||RequestsBlocked()) {logger::info("Call dispatch rejected generation={} mode={} seated={} menu_blocked={} service_busy={}; cleanup not armed",id,static_cast<int>(source),IsSeated(player),MenusOpen(),service::Busy());--pending;return;}
    prompt::Reset();const bool started=service::Request();--pending;
    logger::info("Call entry requested staff approach mode={} accepted={}; no remote menu",static_cast<int>(source),started);
}
void Queue(Mode source) {
    auto player=RE::PlayerCharacter::GetSingleton();if(!player||!player->GetParentCell())return;
    ++pending;const auto id=generation.load();const auto seat=player->GetOccupiedFurniture();const auto cell=player->GetParentCell()->GetFormID();
    SKSE::GetTaskInterface()->AddTask([id,seat,cell,source]{Dispatch(id,seat,cell,source);});
}
void __stdcall Tick(SKSEMenuFramework::Model::EventType event) {
    if(event!=SKSEMenuFramework::Model::kAfterRender||!loaded)return;
    const auto now=std::chrono::steady_clock::now();const auto interval=GetSettings().mode==Mode::framework?50:service::Busy()?100:250;
    if(now-lastUpdate<std::chrono::milliseconds(interval)||updateQueued.exchange(true))return;
    lastUpdate=now;const auto id=generation.load();
    SKSE::GetTaskInterface()->AddTask([id]{
        if(generation.load()==id) {
            const auto now=std::chrono::steady_clock::now();
            if(now-lastServiceUpdate>=std::chrono::milliseconds(service::Busy()?100:250)){lastServiceUpdate=now;topics::TraceDialogueContext();service::Update();}
            auto player=RE::PlayerCharacter::GetSingleton();
            Context context{IsSeated(player),FindInn(player)!=nullptr,player&&player->IsInCombat(),MenusOpen(),RequestsBlocked(),true};
            const auto config=GetSettings();const bool hint=config.mode==Mode::hotkey&&config.showHotkeyHint&&config.hotkey!=0;
            if(config.mode==Mode::framework)RefreshModifier();
            const bool dakAvailable=FrameworkAvailable(),dakHeld=FrameworkHeld();
            const bool focusEnabled=focus::Enabled(config,dakAvailable,dakHeld,context);
            focus::UpdateTarget(player,focusEnabled);
            const auto selection=focusEnabled?CallSelection(player,true):Selection{};
            context.safeTarget=selection.cameraTable||SafeCallTarget(selection.context);
            const bool canDispatch=Trigger(config,0,true,true,dakAvailable,dakHeld,context);
            focus::UpdatePrompt(focusEnabled,selection.cameraTable,canDispatch);
            const bool show=(PromptTrigger(config,prompt::Available(),context)||(hint&&CanCall(context)))&&service::Available();prompt::Update(show);
        }
        updateQueued=false;
    });
}
bool __stdcall Input(RE::InputEvent* event) {
    if(!loaded||!event||event->eventType!=RE::INPUT_EVENT_TYPE::kButton)return false;
    auto button=event->AsButtonEvent();if(!button)return false;
    std::uint32_t key=0;const auto device=button->device.get();
    if(device==RE::INPUT_DEVICE::kKeyboard)key=button->GetIDCode();
    else if(device==RE::INPUT_DEVICE::kMouse)key=256+button->GetIDCode();
    else if(device==RE::INPUT_DEVICE::kGamepad)key=SKSE::InputMap::GamepadMaskToKeycode(button->GetIDCode());
    else return false;
    if(key>=claimed.size())return false;
    const auto modifierKey=FrameworkKey();
    bool dakActive=false;
    {
        std::lock_guard lock(modifierMutex);
        modifier.Observe(modifierKey>0&&modifierKey<282?static_cast<std::uint32_t>(modifierKey):0,key,button->IsPressed());
        dakActive=modifier.Active(dakState&&dakState->value>0);
    }
    const bool owned=claimed[key].load();
    if(button->IsUp()){
        if(owned){claimed[key]=false;return true;}return false;
    }
    if(owned)return true;
    if(!button->IsDown())return false;
    const auto config=GetSettings();
    if(config.mode==Mode::skyprompt)return false;
    const auto controls=RE::ControlMap::GetSingleton();
    // SMF can see raw events before the engine fills userEvent. Resolve the live
    // gameplay mapping as well, so remapped keyboard and controller Activate work.
    const bool activate=button->GetUserEvent()=="Activate"||(controls&&controls->GetMappedKey("Activate",device)==button->GetIDCode());
    if(config.mode==Mode::hotkey?(!config.hotkey||config.hotkey!=key):!activate){
        return false;
    }
    auto player=RE::PlayerCharacter::GetSingleton();
    if(!player||!player->GetParentCell())return false;
    const auto selected=CallSelection(player,config.mode==Mode::framework&&dakActive);
    Context context{IsSeated(player),FindInn(player)!=nullptr,player->IsInCombat(),MenusOpen(),RequestsBlocked(),selected.cameraTable||SafeCallTarget(selected.context)};
    const bool allowed=Trigger(config,key,true,activate,dakState!=nullptr,dakActive,context);
    const bool reserve=ReserveFrameworkPress(config,activate,dakState!=nullptr,dakActive,context);
    if(config.mode==Mode::framework&&context.seated&&context.inInn) {
        const auto camera=RE::PlayerCamera::GetSingleton();
        logger::info("DAK call decision first_person={} key={} modifier={} script_active={} effective_active={} target={:08X} base={:08X} type={} own_seat={} camera_table={} target_safe={} menu_blocked={} busy={} dispatch={} consume={}",
            camera&&camera->IsInFirstPerson(),key,modifierKey,dakState&&dakState->value>0,dakActive,
            selected.reference,selected.base,selected.type,selected.context.occupiedSeat,selected.cameraTable,context.safeTarget,context.menuOpen,context.busy,allowed,allowed||reserve);
    }
    if(!allowed&&!reserve){
        if(config.mode==Mode::hotkey||(context.seated&&context.inInn))
            logger::info("Call key rejected key={} seated={} inn={} combat={} menu_blocked={} busy={} target_safe={}; cleanup not armed",key,context.seated,context.inInn,context.inCombat,context.menuOpen,context.busy,context.safeTarget);
        return false;
    }
    // Own the complete Activate press, including held/up events, even when a
    // recognized DAK service chord cannot enqueue another request yet.
    claimed[key]=true;
    if(!allowed){logger::info("DAK service chord consumed while busy; original Stand Up suppressed; no new request");return true;}
    Queue(config.mode);return true;
}
}
Settings GetSettings(){std::lock_guard lock(settingsMutex);return settings;}
bool SaveSettings(Settings value) {
    if(!Valid(value))return false;std::lock_guard lock(settingsMutex);
    try {
        if(!settingsstore::Save([&](std::string_view before){return UpdatedIni(before,value);})){
            logger::error("Call settings save/readback failed; live settings unchanged");return false;
        }
        settings=value;
        topics::QueueSync();
        logger::info("Call settings saved mode={} hotkey={} priority={} auto_clear={} clear_all={} hide_clear={} hide_restore={}",static_cast<int>(value.mode),value.hotkey,value.prioritizeService,value.autoClearBeforeTalk,value.clearAllTableItems,value.hideClearTableDialogue,value.hideRestoreTableDialogue);return true;
    }catch(const std::exception& e){logger::error("Call settings save failed: {}",e.what());return false;}
}
bool FrameworkAvailable(){return dakState!=nullptr;}
int FrameworkKey(){return dakKey?static_cast<int>(dakKey->value):-1;}
bool FrameworkHeld(){std::lock_guard lock(modifierMutex);return modifier.Active(dakState&&dakState->value>0);}
std::string KeyName(std::uint32_t key){if(!key||key>=282)return T("call_unbound");return SKSE::InputMap::GetKeyName(key);}
bool Busy(){return pending.load()||service::Busy()||(orderGuard&&orderGuard());}
void SetOrderGuard(bool (*guard)()){orderGuard=guard;}
void Reset(){++generation;pending=false;for(auto& key:claimed)key=false;{std::lock_guard lock(modifierMutex);modifier={};}focus::Reset();prompt::Reset();service::Reset();}
bool PromptAvailable(){return prompt::Available();}
bool InteractionBlocked(){return MenusOpen();}
bool RequestExternal(){
    if(!loaded||pending.load()||MenusOpen()||RequestsBlocked())return false;
    prompt::Reset();return service::Request();
}
void RequestFromPrompt() {
    SKSE::GetTaskInterface()->AddTask([]{
        auto player=RE::PlayerCharacter::GetSingleton();Context context{IsSeated(player),FindInn(player)!=nullptr,player&&player->IsInCombat(),MenusOpen(),RequestsBlocked(),true};
        if(!PromptTrigger(GetSettings(),prompt::Available(),context)||!player||!player->GetParentCell())return;
        // Already on the game thread: avoid the old second task/extra frame.
        ++pending;const auto id=generation.load();Dispatch(id,player->GetOccupiedFurniture(),player->GetParentCell()->GetFormID(),Mode::skyprompt);
    });
}
void Initialize() {
    std::string contents;settingsstore::Read(contents);
    settings=Normalize(static_cast<int>(settingsstore::Integer(contents,"Call","Mode",1)),static_cast<int>(settingsstore::Integer(contents,"Call","Hotkey",0)));
    settings.showHotkeyHint=settingsstore::Integer(contents,"Call","ShowHotkeyHint",0)!=0;
    settings.promptUsesHotkey=settingsstore::Integer(contents,"Call","PromptUsesHotkey",0)!=0;
    settings.prioritizeService=settingsstore::Integer(contents,"Call","PrioritizeService",1)!=0;
    settings.autoClearBeforeTalk=settingsstore::Integer(contents,"Call","AutoClearBeforeTalk",0)!=0;
    settings.clearAllTableItems=settingsstore::Integer(contents,"Call","ClearAllTableItems",0)!=0;
    settings.hideClearTableDialogue=settingsstore::Integer(contents,"Call","HideClearTableDialogue",0)!=0;
    settings.hideRestoreTableDialogue=settingsstore::Integer(contents,"Call","HideRestoreTableDialogue",0)!=0;
    topics::Initialize();
    auto data=RE::TESDataHandler::GetSingleton();dakState=data->LookupForm<RE::TESGlobal>(0x801,"Dynamic Activation Key.esp");dakKey=data->LookupForm<RE::TESGlobal>(0x804,"Dynamic Activation Key.esp");
    menuFramework=GetModuleHandleW(L"SKSEMenuFramework.dll");
    if(!menuFramework||!GetProcAddress(menuFramework,"RegisterInpoutEvent")||!GetProcAddress(menuFramework,"GetMainWindow")||!GetProcAddress(menuFramework,"IsAnyBlockingWindowOpened")){logger::warn("Call entry: menu framework input API unavailable");return;}
    settingsWindow=SKSEMenuFramework::GetMainWindow();
    if(!inputRegistration)inputRegistration=SKSEMenuFramework::AddInputEvent(Input);
    if(GetProcAddress(menuFramework,"RegisterEventPriority")&&!updateRegistration)updateRegistration=SKSEMenuFramework::AddEvent(Tick,0.0f);
    prompt::Initialize();focus::Initialize();
    loaded=inputRegistration!=nullptr;
    logger::info("Call service revision=10 mode={} hotkey={} priority={} auto_clear={} DAK={} SkyPrompt={}; native HUD tabletop call prompt; device-state reconciliation recovers missed modifier release; occupied furniture accepted; complete service press consumed",static_cast<int>(settings.mode),settings.hotkey,settings.prioritizeService,settings.autoClearBeforeTalk,dakState!=nullptr,prompt::Available());
}
}
