#pragma once
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <optional>
namespace inn::call {
enum class Mode:int {hotkey=0,framework=1,skyprompt=2};
struct Settings {
    Mode mode=Mode::framework;
    std::uint32_t hotkey=0; // No key is reserved until the player binds one.
    bool showHotkeyHint=false;
    bool promptUsesHotkey=false;
    bool prioritizeService=true;
    bool autoClearBeforeTalk=false; // Legacy INI key; applies only at order entry.
    bool clearAllTableItems=false;
    bool hideClearTableDialogue=false;
    bool hideRestoreTableDialogue=false;
    auto operator<=>(const Settings&) const=default;
};
inline bool Valid(Settings s){return (s.mode==Mode::hotkey||s.mode==Mode::framework||s.mode==Mode::skyprompt)&&s.hotkey<282;}
inline Settings Normalize(int mode,int key){return {mode>=0&&mode<=2?static_cast<Mode>(mode):Mode::framework,key>=0&&key<282?static_cast<std::uint32_t>(key):0};}
struct Context {bool seated{},inInn{},inCombat{},menuOpen{},busy{},safeTarget{};};
// Selection may come from native first person or BTPS. The occupied furniture
// is a valid call surface even when the seat and tabletop share one reference.
struct TargetContext {bool present{},sameCell{},occupiedSeat{},supportedSurface{},inertSurface{};};
inline bool SafeCallTarget(const TargetContext& t) {
    return !t.present||(t.sameCell&&(t.occupiedSeat||(t.supportedSurface&&t.inertSurface)));
}
// DAK's Papyrus global can lag the raw press/release. Once an event for its
// configured key is observed, that key's actual state is authoritative.
struct ModifierState {
    std::uint32_t binding{};
    std::optional<bool> held;
    void Observe(std::uint32_t configured,std::uint32_t eventKey,bool pressed) {
        if(configured!=binding){binding=configured;held.reset();}
        if(configured&&configured==eventKey)held=pressed;
    }
    // SMF can suppress callbacks while a menu control is active. Reconcile the
    // actual device state so a missed Up cannot latch the seated prompt forever.
    void Synchronize(std::uint32_t configured,bool pressed) {
        binding=configured;held=configured>0&&configured<282&&pressed;
    }
    bool Active(bool scriptActive) const {return binding>0&&binding<282&&held.value_or(scriptActive);}
};
inline bool CanCall(const Context& c){return c.seated&&c.inInn&&!c.inCombat&&!c.menuOpen&&!c.busy;}
inline bool ReserveFrameworkPress(Settings s,bool activate,bool dakAvailable,bool dakActive,Context c) {
    // A recognized service chord must not fall through to Stand Up simply
    // because an existing request is temporarily unable to accept another call.
    c.busy=false;
    return Valid(s)&&s.mode==Mode::framework&&activate&&dakAvailable&&dakActive&&c.safeTarget&&CanCall(c);
}
inline bool Trigger(Settings s,std::uint32_t key,bool down,bool activate,bool dakAvailable,bool dakActive,const Context& c) {
    if(!Valid(s)||!down||!CanCall(c))return false;
    if(s.mode==Mode::hotkey)return s.hotkey&&s.hotkey==key;
    if(s.mode==Mode::skyprompt)return false; // SkyPrompt owns and dispatches its buttons.
    return dakAvailable&&dakActive&&activate&&c.safeTarget;
}
inline bool PromptTrigger(Settings s,bool apiAvailable,const Context& c){return s.mode==Mode::skyprompt&&apiAvailable&&CanCall(c);}
// Preserve every unrelated section and setting, including the user's language/UI.
inline std::string UpdatedIni(std::string_view original,Settings s) {
    std::istringstream input{std::string(original)};std::string out,line;bool call=false,written=false;
    const auto trim=[](std::string value){const auto first=value.find_first_not_of(" \t\r");if(first==std::string::npos)return std::string{};return value.substr(first,value.find_last_not_of(" \t\r")-first+1);};
    const auto lower=[](std::string value){for(auto& c:value)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');return value;};
    auto emit=[&]{if(written)return;out+="Mode="+std::to_string(static_cast<int>(s.mode))+"\r\nHotkey="+std::to_string(s.hotkey)+"\r\nShowHotkeyHint="+std::to_string(s.showHotkeyHint)+"\r\nPromptUsesHotkey="+std::to_string(s.promptUsesHotkey)+"\r\nPrioritizeService="+std::to_string(s.prioritizeService)+"\r\nAutoClearBeforeTalk="+std::to_string(s.autoClearBeforeTalk)+"\r\nClearAllTableItems="+std::to_string(s.clearAllTableItems)+"\r\nHideClearTableDialogue="+std::to_string(s.hideClearTableDialogue)+"\r\nHideRestoreTableDialogue="+std::to_string(s.hideRestoreTableDialogue)+"\r\n";written=true;};
    while(std::getline(input,line)) {
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        auto text=trim(line);if(text.starts_with("\xef\xbb\xbf"))text.erase(0,3);
        if(text.starts_with('[')) {
            if(call)emit();call=lower(text)=="[call]";
        }
        if(call){const auto eq=text.find('=');const auto key=lower(trim(text.substr(0,eq)));if(eq!=std::string::npos&&(key=="mode"||key=="hotkey"||key=="showhotkeyhint"||key=="promptuseshotkey"||key=="prioritizeservice"||key=="autoclearbeforetalk"||key=="clearalltableitems"||key=="hidecleartabledialogue"||key=="hiderestoretabledialogue"))continue;}
        out+=line+"\r\n";
    }
    if(!written){if(!call)out+="\r\n[Call]\r\n";emit();}return out;
}
}
