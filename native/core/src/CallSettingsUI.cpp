#include "CallSettingsUI.h"
#include "CallService.h"
#include "Localization.h"
#include "InnServices.h"
#include "ServingSize.h"
#include "InnServiceAPI.h"
#include "SettingsUiWidgets.h"
#include "SKSEMenuFramework.h"
#include <SKSE/InputMap.h>

namespace inn::call::ui {
namespace {
namespace Gui = ImGuiMCP;
constexpr auto captureTimeoutMs = 15000ULL;
constexpr auto pageHeartbeatMs = 1000ULL;
enum class Status : int { none, saved, failed, cancelled, timedOut };
std::atomic<Status> status{Status::none};
std::atomic_bool capturing{}, pageVisible{};
std::atomic<std::uint32_t> capturedKey{}, swallowUntilRelease{};
std::atomic<ULONGLONG> captureStarted{}, lastDraw{};
bool drawnThisFrame{};
bool initialized{};
bool menuTitlesNeedRestart{};
std::string registeredSection,registeredPage;
void (*languageChangedHandler)(){};
SKSEMenuFramework::Model::WindowInterface* mainWindow{};
SKSEMenuFramework::Model::InputEvent* inputListener{};
SKSEMenuFramework::Model::Event* windowListener{};

void CancelCapture() {
    capturing = false;
    capturedKey = 0;
    swallowUntilRelease = 0;
}

bool Save(Settings next) {
    const auto held=swallowUntilRelease.load();
    CancelCapture();
    swallowUntilRelease=held;
    const bool saved = SaveSettings(next);
    status = saved ? Status::saved : Status::failed;
    return saved;
}

std::uint32_t UnifiedKey(RE::ButtonEvent* button) {
    const auto device = button->GetDevice();
    const auto key = button->GetIDCode();
    if (device == RE::INPUT_DEVICE::kKeyboard) return key > 0 && key < 256 ? key : 0;
    if (device == RE::INPUT_DEVICE::kGamepad) {
        const auto mapped = SKSE::InputMap::GamepadMaskToKeycode(key);
        return mapped >= 266 && mapped < 282 ? mapped : 0;
    }
    // A click on the binding button must never become the new binding.
    return 0;
}

bool __stdcall OnInput(RE::InputEvent* event) {
    if (!event || !mainWindow || !mainWindow->IsOpen.load() || !pageVisible.load()) return false;
    const auto now = GetTickCount64();
    if (now - lastDraw.load() > pageHeartbeatMs) return false;
    auto button = event->AsButtonEvent();
    if (!button) return false;
    const auto key = UnifiedKey(button);
    if (!key) return false;
    if (key == swallowUntilRelease.load()) {
        if (button->IsUp()) swallowUntilRelease = 0;
        return true;
    }
    if (!capturing.load()) return false;
    if (now - captureStarted.load() > captureTimeoutMs) {
        CancelCapture();
        status = Status::timedOut;
        return false;
    }
    if (!button->IsDown()) return false;
    if (button->GetDevice() == RE::INPUT_DEVICE::kKeyboard && key == 1) {
        CancelCapture();
        status = Status::cancelled;
        return true;
    }
    capturedKey = key;
    swallowUntilRelease = key;
    capturing = false;
    return true;
}

void __stdcall OnFrameworkEvent(SKSEMenuFramework::Model::EventType event) {
    if (event == SKSEMenuFramework::Model::kCloseMenu) {
        pageVisible = false;
        CancelCapture();
    } else if (event == SKSEMenuFramework::Model::kBeforeRender) {
        drawnThisFrame = false;
    } else if (event == SKSEMenuFramework::Model::kAfterRender) {
        pageVisible = drawnThisFrame;
        if (!drawnThisFrame) CancelCapture();
    }
}

void __stdcall DrawSettings() {
    drawnThisFrame = true;
    pageVisible = true;
    const auto now = GetTickCount64();
    lastDraw = now;
    if (capturing.load() && now - captureStarted.load() > captureTimeoutMs) {
        CancelCapture();
        status = Status::timedOut;
    }
    auto settings = GetSettings();
    if (const auto key = capturedKey.exchange(0); key != 0) {
        if (settings.mode == Mode::hotkey||(settings.mode==Mode::skyprompt&&settings.promptUsesHotkey)) {
            settings.hotkey = key;
            Save(settings);
            settings = GetSettings();
        } else CancelCapture();
    }

    settingsui::Help("callIntro",T("call_settings_intro"));
    const char* choices[]{T("call_mode_hotkey"), T("call_mode_framework"), T("call_mode_skyprompt")};
    int mode = static_cast<int>(settings.mode);
    if (settingsui::layout.showCallMode>.5f&&settingsui::Combo("callMode",T("call_mode_label"), &mode, choices, 3)) {
        settings.mode = static_cast<Mode>(mode);
        Save(settings);
        settings = GetSettings();
    }

    if (settings.mode == Mode::hotkey) {
        settingsui::Help("hotkeyHelp",T("call_hotkey_help"));
        const char* hints[]{T("call_hint_off"),T("call_hint_on")};int hint=settings.showHotkeyHint?1:0;
        if(settingsui::layout.showCallHint>.5f&&settingsui::Combo("callHint",T("call_hint_label"),&hint,hints,2)){settings.showHotkeyHint=hint!=0;Save(settings);settings=GetSettings();}
        settingsui::Help("hintHelp",T("call_hint_help"));
    }else if(settings.mode==Mode::skyprompt){
        const char* sources[]{T("call_prompt_keys_framework"),T("call_prompt_keys_own")};int source=settings.promptUsesHotkey?1:0;
        if(settingsui::layout.showPromptSource>.5f&&settingsui::Combo("promptSource",T("call_prompt_keys_label"),&source,sources,2)){settings.promptUsesHotkey=source!=0;Save(settings);settings=GetSettings();}
    }
    if(settingsui::layout.showBinding>.5f&&(settings.mode==Mode::hotkey||(settings.mode==Mode::skyprompt&&settings.promptUsesHotkey))){
        const auto name = settings.hotkey ? KeyName(settings.hotkey) : std::string(T("call_key_unbound"));
        Gui::Text("%s: %s", T("call_current_binding"), name.c_str());
        if (capturing.load()) {
            Gui::TextWrapped("%s", T("call_capture_prompt"));
            if (Gui::Button(T("call_capture_cancel"))) {
                CancelCapture();
                status = Status::cancelled;
            }
        } else {
            if (Gui::Button(T("call_capture_start"))) {
                CancelCapture();
                captureStarted = GetTickCount64();
                capturing = true;
                status = Status::none;
            }
            Gui::SameLine();
            Gui::BeginDisabled(settings.hotkey == 0);
            if (Gui::Button(T("call_capture_clear"))) {
                settings.hotkey = 0;
                Save(settings);
            }
            Gui::EndDisabled();
        }
    } else if(settings.mode==Mode::framework) {
        CancelCapture();
        settingsui::Help("frameworkHelp",T("call_framework_help"));
        Gui::TextWrapped("%s", T(FrameworkAvailable() ? "call_framework_available" : "call_framework_missing"));
        const int key = FrameworkKey();
        const auto name = key > 0 && key < 282 ? KeyName(static_cast<std::uint32_t>(key)) : std::string(T("call_key_unbound"));
        Gui::Text("%s: %s", T("call_framework_binding"), name.c_str());
    }
    if(settings.mode==Mode::skyprompt){
        if(!settings.promptUsesHotkey)CancelCapture();
        settingsui::Help("promptHelp",T("call_skyprompt_help"));
        Gui::TextWrapped("%s",T(PromptAvailable()?"call_skyprompt_available":"call_skyprompt_missing"));
    }

    Gui::Separator();
    const char* priorities[]{T("call_priority_normal"),T("call_priority_enabled")};int priority=settings.prioritizeService?1:0;
    if(settingsui::layout.showPriority>.5f&&settingsui::Combo("priority",T("call_priority_label"),&priority,priorities,2)){settings.prioritizeService=priority!=0;Save(settings);settings=GetSettings();}
    settingsui::Help("priorityHelp",T("call_priority_help"));
    settingsui::Help("contextHelp",T("call_settings_context"));
    settingsui::Help("approachHelp",T("call_approach_help"));
    settingsui::Help("persistenceHelp",T("call_settings_persistence"));
    if (Busy()) Gui::TextWrapped("%s", T("call_service_busy"));
}
void __stdcall DrawCleanupSettings() {
    auto settings=GetSettings();
    settingsui::Help("cleanupIntro",T("cleanup_settings_intro"));
    const char* cleaning[]{T("call_autoclear_off"),T("call_autoclear_on")};
    int clear=settings.autoClearBeforeTalk?1:0;
    if(settingsui::layout.showAutoClear>.5f&&settingsui::Combo("autoClear",T("call_autoclear_label"),&clear,cleaning,2)) {
        settings.autoClearBeforeTalk=clear!=0;Save(settings);settings=GetSettings();
    }
    settingsui::Help("autoClearHelp",T("call_autoclear_help"));
    const char* scopes[]{T("cleanup_scope_meal"),T("cleanup_scope_all")};
    int scope=settings.clearAllTableItems?1:0;
    if(settingsui::layout.showScope>.5f&&settingsui::Combo("scope",T("cleanup_scope_label"),&scope,scopes,2)) {
        settings.clearAllTableItems=scope!=0;Save(settings);
    }
    settingsui::Help("scopeHelp",T("cleanup_scope_help"));
    settingsui::Help("restoreHelp",T("cleanup_restore_help"));
    Gui::Separator();
    const auto hideClear=std::string(T("cleanup_hide_clear_label"))+"###hideClearTableDialogue";
    if(Gui::Checkbox(hideClear.c_str(),&settings.hideClearTableDialogue)){
        Save(settings);settings=GetSettings();
    }
    settingsui::Hint(T("cleanup_hide_clear_help"));
    const auto hideRestore=std::string(T("cleanup_hide_restore_label"))+"###hideRestoreTableDialogue";
    if(Gui::Checkbox(hideRestore.c_str(),&settings.hideRestoreTableDialogue)){
        Save(settings);settings=GetSettings();
    }
    settingsui::Hint(T("cleanup_hide_restore_help"));
}
bool RefreshMenuLabels() {
    using Rename=bool (*)(const char*,const char*);
    const auto rename=reinterpret_cast<Rename>(GetProcAddress(menuFramework,"RenameSection"));
    bool complete=rename!=nullptr;
    const std::string nextPage=T("call_settings_page"),nextSection=T("call_settings_section");
    if(rename&&registeredPage!=nextPage) {
        const auto path=registeredSection+"/"+registeredPage;
        if(rename(path.c_str(),nextPage.c_str()))registeredPage=nextPage;
        else complete=false;
    }
    complete=serving::RefreshSettingsPageLabel()&&complete;
    // The bridge queues its child rename synchronously before the shared parent.
    std::uint32_t chinese=strings.IsChinese()?1:0;
    SKSE::GetMessagingInterface()->Dispatch(innservice::api::languageChanged,&chinese,sizeof(chinese),nullptr);
    if(rename&&registeredSection!=nextSection) {
        if(rename(registeredSection.c_str(),nextSection.c_str()))registeredSection=nextSection;
        else complete=false;
    }
    return complete;
}
void __stdcall DrawGeneral() {
    settingsui::Begin();
    drawnThisFrame=true;pageVisible=true;lastDraw=GetTickCount64();
    int language=strings.IsChinese()?1:0;
    const char* languages[]{"English","中文"};
    if(settingsui::Combo("innLanguage",T("general_language_label"),&language,languages,2)) {
        CancelCapture();
        if(SaveLanguage(language==1)) {
            status=Status::saved;
            menuTitlesNeedRestart=!RefreshMenuLabels();
            if(languageChangedHandler)languageChangedHandler();
        }else status=Status::failed;
    }
    settingsui::Help("languageHelp",T("general_language_help"));
    if(menuTitlesNeedRestart)Gui::TextWrapped("%s",T("general_language_restart"));
    Gui::Separator();
    const auto callHeading=std::string(T("general_call_heading"))+"###innCallHotkeys";
    const auto cleanupHeading=std::string(T("general_cleanup_heading"))+"###innTableCleanup";
    const auto callSection=[&]{const bool open=Gui::CollapsingHeader(callHeading.c_str(),settingsui::layout.callExpanded>.5f?Gui::ImGuiTreeNodeFlags_DefaultOpen:0);settingsui::Hint(T("call_settings_intro"));if(open)DrawSettings();else CancelCapture();};
    const auto cleanupSection=[&]{const bool open=Gui::CollapsingHeader(cleanupHeading.c_str(),settingsui::layout.cleanupExpanded>.5f?Gui::ImGuiTreeNodeFlags_DefaultOpen:0);settingsui::Hint(T("cleanup_settings_intro"));if(open)DrawCleanupSettings();};
    if(settingsui::layout.callFirst>.5f){callSection();settingsui::Gap();cleanupSection();}
    else {cleanupSection();settingsui::Gap();callSection();}
    switch(status.load()) {
    case Status::saved: Gui::TextWrapped("%s",T("call_settings_saved"));break;
    case Status::failed: Gui::TextWrapped("%s",T("call_settings_save_failed"));break;
    case Status::cancelled: Gui::TextWrapped("%s",T("call_capture_cancelled"));break;
    case Status::timedOut: Gui::TextWrapped("%s",T("call_capture_timeout"));break;
    default:break;
    }
    settingsui::End();
}
}

void SetLanguageChangedHandler(void (*handler)()){languageChangedHandler=handler;}

bool Initialize() {
    if (initialized) return true;
    // SKSEMenuFramework.h intentionally keeps its module handle local to each TU.
    menuFramework = GetModuleHandleW(L"SKSEMenuFramework.dll");
    if (!menuFramework) return false;
    if(!settingsui::Available())return false;
    const bool layoutLoaded=settingsui::Load();
    logger::info("SKSE settings layout loaded={} help_mode={} control_width={} font_scale={} labels_above={} cwd={}",layoutLoaded,settingsui::layout.helpMode,settingsui::layout.controlWidth,settingsui::layout.fontScale,settingsui::layout.labelsAbove,std::filesystem::current_path().string());
    constexpr const char* exports[]{
        "AddSectionItem", "GetMainWindow", "RegisterInpoutEvent", "UnregisterInputEvent",
        "RegisterEventPriority", "UnregisterEvent", "igCombo_Str_arr", "igButton", "igTextV",
        "igTextWrappedV", "igSeparator", "igSameLine", "igBeginDisabled", "igEndDisabled",
        "igCollapsingHeader_TreeNodeFlags", "igCheckbox"
    };
    for (const auto name : exports) if (!GetProcAddress(menuFramework, name)) {
        logger::warn("Call settings page unavailable: missing SKSE Menu Framework export {}", name);
        return false;
    }
    mainWindow = SKSEMenuFramework::GetMainWindow();
    if (!mainWindow) return false;
    inputListener = SKSEMenuFramework::AddInputEvent(OnInput);
    windowListener = SKSEMenuFramework::AddEvent(OnFrameworkEvent, 0.0f);
    registeredSection=T("call_settings_section");registeredPage=T("call_settings_page");
    SKSEMenuFramework::SetSection(registeredSection);
    SKSEMenuFramework::AddSectionItem(registeredPage, DrawGeneral);
    initialized = true;
    logger::info("General settings page registered: language, call hotkeys, table cleanup; key capture limited to its visible call section");
    return true;
}

void Reset() {
    pageVisible = false;
    CancelCapture();
    status = Status::none;
}
}
