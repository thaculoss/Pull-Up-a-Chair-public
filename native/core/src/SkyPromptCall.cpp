#include "PCH.h"
#include "SkyPromptCall.h"
#include "CallService.h"
#include "Localization.h"
#include "SkyPrompt/API.hpp"
#include <SKSE/InputMap.h>
namespace inn::call::prompt {
namespace {
SkyPromptAPI::ClientID client{};
std::array<SkyPromptAPI::Prompt,1> prompts;
bool showing{};
std::atomic_bool holding{};
Settings lastSettings{};
std::array<std::pair<RE::INPUT_DEVICE,SkyPromptAPI::ButtonID>,1> buttons;
std::chrono::steady_clock::time_point lastSent{};
class Sink:public SkyPromptAPI::PromptSink {
public:
    std::span<const SkyPromptAPI::Prompt> GetPrompts() const override{return prompts;}
    void ProcessEvent(SkyPromptAPI::PromptEvent event) const override {
        if(event.prompt.eventID!=7420||event.prompt.actionID!=0)return;
        if(event.type==SkyPromptAPI::kDown)holding=true;
        if(event.type==SkyPromptAPI::kUp||event.type==SkyPromptAPI::kAccepted||event.type==SkyPromptAPI::kDeclined||event.type==SkyPromptAPI::kTimeout||event.type==SkyPromptAPI::kRemovedByMod)holding=false;
        if(event.type==SkyPromptAPI::PromptEventType::kAccepted&&event.prompt.type==SkyPromptAPI::kHold&&event.prompt.eventID==7420&&event.prompt.actionID==0)
            if(GetSettings().mode==Mode::skyprompt)RequestFromPrompt();
    }
};
Sink sink;
}
void Initialize() {
    if(client)return;auto module=GetModuleHandleW(L"SkyPrompt.dll");
    if(!module)return;
    for(auto name:{"ProcessRequestClientID","ProcessSendPrompt","ProcessRemovePrompt"})if(!GetProcAddress(module,name)){logger::warn("SkyPrompt call entry: missing {}",name);return;}
    client=SkyPromptAPI::RequestClientID();
    if(client)prompts[0]=SkyPromptAPI::Prompt{T("call_prompt"),7420,0,SkyPromptAPI::kHold};
    logger::info("SkyPrompt call client={} (API 2.0); hold action or non-blocking dedicated-key hint",client);
}
bool Available(){return client!=0;}
void Update(bool visible) {
    if(!client)return;
    if(!visible){Reset();return;}
    const auto config=GetSettings();const bool hint=config.mode==Mode::hotkey;
    const bool custom=hint||config.promptUsesHotkey;
    if(custom&&!config.hotkey){Reset();return;}
    if(!showing||config!=lastSettings) {
        Reset();lastSettings=config;
        prompts[0]=SkyPromptAPI::Prompt{T(hint?"call_prompt":"call_prompt_hold"),7420,0,
            hint?SkyPromptAPI::kHint:SkyPromptAPI::kHold};
        if(custom) {
            auto device=RE::INPUT_DEVICE::kKeyboard;auto code=config.hotkey;
            if(code>=266){device=RE::INPUT_DEVICE::kGamepad;code=SKSE::InputMap::GamepadKeycodeToMask(code);}
            else if(code>=256){device=RE::INPUT_DEVICE::kMouse;code-=256;}
            buttons[0]={device,code};prompts[0].button_key=buttons;
        }
    }
    const auto now=std::chrono::steady_clock::now();
    // Installed SkyPrompt has a five-second lifetime; refresh our own prompt only.
    if(!showing||(!holding&&now-lastSent>=std::chrono::seconds(2))) {
        showing=SkyPromptAPI::SendPrompt(&sink,client);lastSent=now;
    }
}
void Reset(){if(client&&showing)SkyPromptAPI::RemovePrompt(&sink,client);showing=false;holding=false;}
}
