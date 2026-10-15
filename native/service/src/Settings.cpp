#include "PCH.h"
#include "Settings.h"
#include "ServiceManager.h"
#include "BindingPolicy.h"
#include "BindingStore.h"
#include "SettingsUiLayout.h"
namespace innservice {
namespace {
constexpr auto userConfig=".\\Data\\SKSE\\Plugins\\PullUpAChairService_user.ini";
inn::Localization orderingStrings;
std::string menuSection;
std::mutex writeMutex;
}
const char* MenuSection(){return menuSection.c_str();}
void ApplyMenuLanguage(bool value) {
    if(!UsesCore())return;
    const std::string oldPage=Text("page");
    chinese=value;inn::strings.UseChinese(value);orderingStrings.UseChinese(value);
    using Rename=bool (*)(const char*,const char*);
    const auto framework=GetModuleHandleW(L"SKSEMenuFramework.dll");
    const auto rename=framework?reinterpret_cast<Rename>(GetProcAddress(framework,"RenameSection")):nullptr;
    if(rename) {
        const auto path=menuSection+"/"+oldPage;
        if(oldPage!=Text("page")&&!rename(path.c_str(),Text("page")))logger::warn("Stock page rename unavailable until restart");
        menuSection=orderingStrings.Text("call_settings_section");
    }
    logger::info("Stock page language synchronized={}",value?"zh":"en");
}
void LoadSettings() {
    const bool layoutLoaded=inn::settingsui::Load();
    logger::info("SKSE stock layout loaded={} help_mode={} list_height={}",layoutLoaded,inn::settingsui::layout.helpMode,inn::settingsui::layout.stockHeight);
    constexpr auto path=".\\Data\\SKSE\\Plugins\\PullUpAChairService.ini";
    const bool core=UsesCore();
    const auto languageConfig=core?".\\Data\\SKSE\\Plugins\\PullUpAChairOrdering.ini":path;
    char language[32]{};GetPrivateProfileStringA("General","Language",core?"en":"auto",language,sizeof(language),languageConfig);
    std::string lang=language;
    if(lang=="auto") {
        auto collection=RE::INISettingCollection::GetSingleton();auto setting=collection?collection->GetSetting("sLanguage:General"):nullptr;
        lang=setting&&setting->GetString()?setting->GetString():"english";
        std::transform(lang.begin(),lang.end(),lang.begin(),[](unsigned char c){return char(std::tolower(c));});
        lang=lang.find("chinese")!=std::string::npos||lang.find("taiwan")!=std::string::npos||lang.starts_with("zh")?"zh":"en";
    }
    chinese=lang=="zh";inn::strings.Load("Data/SKSE/Plugins/PullUpAChairService/locales",lang);
    if(core) {
        // Read the same translated parent as the core, including custom locale overlays.
        if(!orderingStrings.Load("Data/SKSE/Plugins/PullUpAChairOrdering/locales",lang))logger::error("Ordering menu English fallback missing");
        menuSection=orderingStrings.Text("call_settings_section");
    }else menuSection=Text("section");
    const auto bindings=ReadBindings(path,userConfig);
    settings.key=bindings.keyboard;settings.chord=bindings.gamepad;
    settings.radius=float(std::clamp(int(GetPrivateProfileIntA("Service","SearchRadius",1800,path)),100,5000));
    settings.arrival=float(std::clamp(int(GetPrivateProfileIntA("Service","ArrivalDistance",145,path)),80,200));
    settings.approachTimeout=float(std::clamp(int(GetPrivateProfileIntA("Service","ApproachTimeout",30,path)),5,120));
    settings.readyTimeout=float(std::clamp(int(GetPrivateProfileIntA("Service","ReadyTimeout",45,path)),10,300));
    settings.autoTalk=GetPrivateProfileIntA("Service","AutoTalk",1,path)!=0;
    logger::info("Language={} keyboard={} gamepadChord={:X} searchRadius={} arrival={}",lang,settings.key.load(),settings.chord.load(),settings.radius,settings.arrival);
    logger::info("SKSE menu parent={} page={} language source={}",menuSection,Text("page"),core?"PullUpAChairOrdering.ini":"PullUpAChairService.ini");
}
bool SetKeyboardBinding(int key) {
    if(key<0||key>255||key==1)return false;
    std::lock_guard lock(writeMutex);
    if(!WriteKeyboardBinding(userConfig,key))return false;
    settings.key=key;++bindingRevision;logger::info("Keyboard binding saved={}",key);return true;
}
bool SetGamepadBinding(std::uint32_t mask) {
    if(mask&~supportedPad)return false;
    std::lock_guard lock(writeMutex);
    if(!WriteGamepadBinding(userConfig,mask))return false;
    settings.chord=mask;++bindingRevision;logger::info("Gamepad binding saved={:X}",mask);return true;
}
bool ResetBindings(){const bool keyboard=SetKeyboardBinding(37);const bool pad=SetGamepadBinding(0x300);return keyboard&&pad;}
std::string KeyboardName(int key) {
    if(!key)return Text("unbound");
    wchar_t name[128]{};const LONG param=((key&0x7F)<<16)|((key&0x80)?1<<24:0);
    if(GetKeyNameTextW(param,name,128)>0) {
        const int count=WideCharToMultiByte(CP_UTF8,0,name,-1,nullptr,0,nullptr,nullptr);
        std::string result(count,'\0');WideCharToMultiByte(CP_UTF8,0,name,-1,result.data(),count,nullptr,nullptr);result.pop_back();return result;
    }
    return std::string(Text("key"))+" "+std::to_string(key);
}
std::string GamepadName(std::uint32_t mask) {
    if(!mask)return Text("unbound");
    const std::pair<std::uint32_t,const char*> buttons[]={{1,"pad_up"},{2,"pad_down"},{4,"pad_left"},{8,"pad_right"},
        {16,"Menu"},{32,"View"},{64,"L3"},{128,"R3"},{256,"LB"},{512,"RB"},{4096,"A"},{8192,"B"},{16384,"X"},{32768,"Y"},{65536,"LT"},{131072,"RT"}};
    std::string result;
    for(const auto& [bit,label]:buttons)if(mask&bit){if(!result.empty())result+=" + ";result+=bit<16?Text(label):label;}
    return result;
}
}
