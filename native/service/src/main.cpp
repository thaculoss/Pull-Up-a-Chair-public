#include "PCH.h"
#include "RuntimePolicy.h"
#include "Settings.h"
#include "Supply.h"
#include "ServiceManager.h"
#include "BindingUI.h"
#include "BindingPolicy.h"
#include "SKSEMenuFramework.h"
#include "SettingsUiWidgets.h"
namespace ImGui=ImGuiMCP;
namespace innservice {
bool BlockingMenus() {
    auto module=GetModuleHandleW(L"SKSEMenuFramework.dll");
    auto function=module?reinterpret_cast<bool(*)()>(GetProcAddress(module,"IsAnyBlockingWindowOpened")):nullptr;
    return function&&function();
}
}
namespace {
innservice::PadChord pad;
unsigned inputRevision{};
class Input final:public RE::BSTEventSink<RE::InputEvent*> {
    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* events,RE::BSTEventSource<RE::InputEvent*>*) override {
        if(!events||innservice::UsesCore())return RE::BSEventNotifyControl::kContinue;
        for(auto event=*events;event;event=event->next)innservice::CaptureInput(event);
        const auto revision=innservice::bindingRevision.load();if(inputRevision!=revision){pad={};inputRevision=revision;}
        auto ui=RE::UI::GetSingleton();if((ui&&ui->GameIsPaused())||innservice::BlockingMenus()){pad={};return RE::BSEventNotifyControl::kContinue;}
        auto player=RE::PlayerCharacter::GetSingleton();if(!innservice::Seated(player)||!innservice::FindInn(player)){pad={};return RE::BSEventNotifyControl::kContinue;}
        for(auto event=*events;event;event=event->next)if(auto button=event->AsButtonEvent()) {
            const auto device=button->GetDevice();const auto id=button->GetIDCode();
            if(device==RE::INPUT_DEVICE::kKeyboard&&button->IsDown()&&innservice::settings.key.load()&&id==static_cast<unsigned>(innservice::settings.key.load()))innservice::QueueRequest();
            else if(device==RE::INPUT_DEVICE::kGamepad) {
                const auto mask=innservice::PadButton(id);
                if(mask&&pad.Update(mask,button->IsPressed(),innservice::settings.chord.load()))innservice::QueueRequest();
            }
        }
        return RE::BSEventNotifyControl::kContinue;
    }
} input;
class Furniture final:public RE::BSTEventSink<RE::TESFurnitureEvent> {
    RE::BSEventNotifyControl ProcessEvent(const RE::TESFurnitureEvent* event,RE::BSTEventSource<RE::TESFurnitureEvent>*) override {
        if(!innservice::UsesCore()&&event&&event->actor.get()==RE::PlayerCharacter::GetSingleton()&&event->type.any(RE::TESFurnitureEvent::FurnitureEventType::kExit)){pad={};innservice::QueueCancel();}
        return RE::BSEventNotifyControl::kContinue;
    }
} furniture;
void __stdcall Draw() {
    using namespace innservice;
    const auto state=CurrentView();
    inn::settingsui::Begin();
    ImGui::TextUnformatted(Text("intro"));
    if(UsesCore()){if(inn::settingsui::layout.stockStatus>.5f)ImGui::TextUnformatted(Text(BackendStatus()));}
    else {ImGui::TextUnformatted(Text("input_hint"));DrawBindings();}
    if(inn::settingsui::layout.stockActions>.5f){if(ImGui::Button(Text("call")))QueueRequest(true);
    ImGui::SameLine();if(ImGui::Button(Text("cancel")))QueueCancel();
    ImGui::SameLine();if(ImGui::Button(Text("refresh")))QueueRefresh();}
    ImGui::Separator();ImGui::Text("%s: %s",Text("inn"),state.innName.c_str());
    if(inn::settingsui::layout.stockOwners>.5f){ImGui::Text("%s: %s",Text("server"),state.serverName.c_str());
    ImGui::Text("%s: %s",Text("merchant"),state.merchantName.c_str());}
    if(inn::settingsui::layout.stockStatus>.5f)ImGui::TextUnformatted(Text(state.status));
    if(state.supply.status==static_cast<unsigned>(api::Status::ambiguousStock))ImGui::TextUnformatted(Text("stock_ambiguous"));
    else if(!state.supply.chest)ImGui::TextUnformatted(Text("stock_missing"));
    else {
        ImGui::Text("%s: %zu",Text("stock_types"),state.stock.size());
        if(ImGui::BeginChild("inn_stock",{0,inn::settingsui::layout.stockHeight},0,0))for(const auto& [name,count]:state.stock)ImGui::Text("%s  x%d",name.c_str(),count);
        ImGui::EndChild();
    }
    inn::settingsui::End();
}
bool Bind(RE::BSScript::IVirtualMachine* vm) {
    vm->RegisterFunction("CallWaiter","PullUpAChairService",+[](RE::StaticFunctionTag*){innservice::QueueRequest();});
    vm->RegisterFunction("CancelService","PullUpAChairService",+[](RE::StaticFunctionTag*){innservice::QueueCancel();});
    vm->RegisterFunction("RefreshSupply","PullUpAChairService",+[](RE::StaticFunctionTag*){innservice::QueueRefresh();});
    vm->RegisterFunction("IsReleaseCurrent","PullUpAChairService",+[](RE::StaticFunctionTag*,int token){return innservice::IsReleaseCurrent(token);});
    vm->RegisterFunction("ReleaseComplete","PullUpAChairService",+[](RE::StaticFunctionTag*,int token){innservice::ReleaseComplete(token);});
    return true;
}
void Message(SKSE::MessagingInterface::Message* message) {
    if(message->type==SKSE::MessagingInterface::kPostPostLoad){innservice::DiscoverBackend();}
    else if(message->type==SKSE::MessagingInterface::kDataLoaded) {
        innservice::DiscoverBackend();innservice::LoadSettings();innservice::InitWorker();
        if(!innservice::UsesCore()){
            innservice::LoadRoles();innservice::InstallBindingCapture();
            RE::BSInputDeviceManager::GetSingleton()->AddEventSink(&input);
            RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink(&furniture);
        }
        menuFramework=GetModuleHandleW(L"SKSEMenuFramework.dll");
        if(menuFramework&&GetProcAddress(menuFramework,"AddSectionItem")&&inn::settingsui::Available()) {
            SKSEMenuFramework::SetSection(innservice::MenuSection());
            SKSEMenuFramework::AddSectionItem(innservice::Text("page"),Draw);
        }
        // Alias migration starts only after a successful load/new game.
        logger::info("Inn service compatibility facade initialized; mode={}; gameplay validation required",innservice::UsesCore()?"ordering backend (legacy AI/input disabled)":"standalone fallback");
    }else if(message->type==SKSE::MessagingInterface::kPreLoadGame){innservice::CancelBindingCapture();innservice::Revert();}
    else if(message->type==SKSE::MessagingInterface::kNewGame) {
        innservice::Revert();innservice::SessionLoaded();
    }else if(message->type==SKSE::MessagingInterface::kPostLoadGame&&message->data)
        innservice::SessionLoaded();
}
const innservice::api::API api{sizeof(innservice::api::API),1,innservice::ResolveAPI,innservice::InventoryAPI,
    innservice::Request,innservice::CopyState,innservice::Retain,innservice::Release,
    +[]{innservice::QueueCancel();}};
}
extern "C" __declspec(dllexport) const innservice::api::API* PullUpAChairService_GetAPI(std::uint32_t requested) {
    return requested==innservice::api::version?&api:nullptr;
}
SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    const auto version=skse->RuntimeVersion();
    if(!inn::Supported({version.major(),version.minor(),version.patch(),version.build()}))return false;
    SKSE::Init(skse);auto path=SKSE::log::log_directory();if(!path)return false;*path/="PullUpAChairService.log";
    auto sink=std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(),true);
    spdlog::set_default_logger(std::make_shared<spdlog::logger>("InnService",sink));
    spdlog::set_level(spdlog::level::info);spdlog::flush_on(spdlog::level::info);
    SKSE::GetPapyrusInterface()->Register(Bind);
    auto io=SKSE::GetSerializationInterface();io->SetUniqueID(0x494E5356);io->SetSaveCallback(innservice::Save);io->SetLoadCallback(innservice::Load);
    io->SetRevertCallback([](SKSE::SerializationInterface*){innservice::Revert();});
    SKSE::GetMessagingInterface()->RegisterListener(Message);
    logger::info("Pull Up a Chair Service 0.9 beta vrtrial1 runtime={} mode={}; configurable SKSE stock layout and live interface language; game testing pending",version.string(),inn::ExperimentalVR({version.major(),version.minor(),version.patch(),version.build()})?"experimental VR (unverified)":"flat runtime");
    return true;
}
