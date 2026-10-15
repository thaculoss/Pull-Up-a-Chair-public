#include "PCH.h"
#include "BindingUI.h"
#include "BindingPolicy.h"
#include "Settings.h"
#include "SKSEMenuFramework.h"
namespace ImGui=ImGuiMCP;
namespace innservice {
namespace {
std::mutex captureMutex;
BindingRecorder recorder;
std::chrono::steady_clock::time_point started,lastDraw;
std::string feedback;
bool available{};
void Begin(CaptureDevice mode){std::lock_guard lock(captureMutex);recorder.Begin(mode);started=lastDraw=std::chrono::steady_clock::now();feedback.clear();}
bool __stdcall CaptureCallback(RE::InputEvent* event){return CaptureInput(event);}
}
bool CaptureInput(RE::InputEvent* event) {
    if(!event)return false;
    auto button=event->AsButtonEvent();if(!button)return false;
    const auto device=button->GetDevice();
    if(device!=RE::INPUT_DEVICE::kKeyboard&&device!=RE::INPUT_DEVICE::kGamepad)return false;
    std::lock_guard lock(captureMutex);if(recorder.mode==CaptureDevice::none)return false;
    const auto now=std::chrono::steady_clock::now();
    if(now-started>20s||now-lastDraw>750ms||!BlockingMenus()){recorder.Cancel();feedback="binding_cancelled";return false;}
    const auto result=recorder.Event(device==RE::INPUT_DEVICE::kKeyboard?CaptureDevice::keyboard:CaptureDevice::gamepad,
        button->GetIDCode(),button->IsDown(),button->IsUp());
    if(result.action==CaptureAction::complete) {
        const bool saved=result.device==CaptureDevice::keyboard?SetKeyboardBinding(static_cast<int>(result.binding)):SetGamepadBinding(result.binding);
        feedback=saved?"binding_saved":"binding_save_failed";
    }else if(result.action==CaptureAction::cancelled)feedback="binding_cancelled";
    return true; // Keep capture events out of menu navigation and gameplay.
}
void CancelBindingCapture(){std::lock_guard lock(captureMutex);if(recorder.mode!=CaptureDevice::none)feedback="binding_cancelled";recorder.Cancel();}
void InstallBindingCapture() {
    menuFramework=GetModuleHandleW(L"SKSEMenuFramework.dll");
    auto registerInput=menuFramework?reinterpret_cast<SKSEMenuFramework::Model::RegisterInputEventFuction>(GetProcAddress(menuFramework,"RegisterInpoutEvent")):nullptr;
    available=registerInput!=nullptr;if(registerInput)registerInput(CaptureCallback);
}
void DrawBindings() {
    CaptureDevice mode;std::string message;
    {
        std::lock_guard lock(captureMutex);
        lastDraw=std::chrono::steady_clock::now();
        if(recorder.mode!=CaptureDevice::none&&(std::chrono::steady_clock::now()-started>20s||!BlockingMenus())) {
            recorder.Cancel();feedback="binding_cancelled";
        }
        mode=recorder.mode;message=feedback;
    }
    ImGui::Separator();ImGui::TextUnformatted(Text("bindings"));
    ImGui::Text("%s: %s",Text("keyboard"),KeyboardName(settings.key.load()).c_str());
    ImGui::BeginDisabled(!available||mode!=CaptureDevice::none);
    if(ImGui::Button(Text("record_keyboard")))Begin(CaptureDevice::keyboard);
    ImGui::EndDisabled();ImGui::SameLine();if(ImGui::Button(Text("disable_keyboard"))){CancelBindingCapture();SetKeyboardBinding(0);}
    ImGui::Text("%s: %s",Text("gamepad"),GamepadName(settings.chord.load()).c_str());
    ImGui::BeginDisabled(!available||mode!=CaptureDevice::none);
    if(ImGui::Button(Text("record_gamepad")))Begin(CaptureDevice::gamepad);
    ImGui::EndDisabled();ImGui::SameLine();if(ImGui::Button(Text("disable_gamepad"))){CancelBindingCapture();SetGamepadBinding(0);}
    if(ImGui::Button(Text("default_bindings"))){CancelBindingCapture();ResetBindings();}
    if(mode!=CaptureDevice::none) {
        ImGui::TextWrapped("%s",Text(mode==CaptureDevice::keyboard?"record_keyboard_hint":"record_gamepad_hint"));
        if(ImGui::Button(Text("cancel_recording"))){CancelBindingCapture();message="binding_cancelled";}
    }
    if(!message.empty())ImGui::TextWrapped("%s",Text(message));
    if(!available)ImGui::TextWrapped("%s",Text("capture_unavailable"));
    ImGui::TextWrapped("%s",Text("bindings_hint"));
}
}
