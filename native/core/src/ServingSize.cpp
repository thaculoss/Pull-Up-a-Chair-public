#include "PCH.h"
#include "ServingSize.h"
#include "SettingsStore.h"
#include "Localization.h"
#include "SKSEMenuFramework.h"
#include "SettingsUiWidgets.h"
namespace inn::serving {
namespace {

std::mutex mutex;
Settings settings;
bool initialized{};
std::string registeredSection,registeredPage;
int status{};
bool DrawTarget(const char* id,const char* label,float& diameter,bool enabled) {
    const char* sizes[]{T("serving_size_soup"),T("serving_size_medium"),T("serving_size_large"),T("serving_size_custom")};
    int selected=3;constexpr float values[]{14.f,18.f,22.f};
    for(int i=0;i<3;++i)if(std::abs(diameter-values[i])<.01f)selected=i;
    ImGuiMCP::BeginDisabled(!enabled);
    const bool changed=settingsui::Combo(id,label,&selected,sizes,selected==3?4:3)&&selected<3;
    ImGuiMCP::EndDisabled();
    if(changed)diameter=values[selected];return changed;
}
void __stdcall Draw() {
    settingsui::Begin();
    auto value=GetSettings();
    settingsui::Help("servingIntro",T("serving_size_intro"));
    const char* states[]{T("serving_size_off"),T("serving_size_on")};int enabled=value.compactMain?1:0;
    const auto main=[&]{if(settingsui::Combo("compactMain",T("serving_size_enable"),&enabled,states,2)) {
        value.compactMain=enabled!=0;status=SaveSettings(value)?1:-1;value=GetSettings();
    }
    if(DrawTarget("mainTarget",T("serving_size_target"),value.targetDiameter,value.compactMain)){status=SaveSettings(value)?1:-1;value=GetSettings();}};
    int desserts=value.compactDesserts?1:0;
    const auto dessert=[&]{if(settingsui::Combo("compactDesserts",T("serving_dessert_enable"),&desserts,states,2)) {
        value.compactDesserts=desserts!=0;status=SaveSettings(value)?1:-1;value=GetSettings();
    }
    if(DrawTarget("dessertTarget",T("serving_dessert_target"),value.targetDessertDiameter,value.compactDesserts)){status=SaveSettings(value)?1:-1;value=GetSettings();}};
    if(settingsui::layout.dessertsFirst>.5f){dessert();settingsui::Gap();main();}else {main();settingsui::Gap();dessert();}
    settingsui::Help("servingHelp",T("serving_size_help"));
    if(status==1)ImGuiMCP::TextWrapped("%s",T("serving_size_saved"));
    else if(status==-1)ImGuiMCP::TextWrapped("%s",T("call_settings_save_failed"));
    settingsui::End();
}
}
void LoadSettings() {
    Settings value;
    std::string contents;settingsstore::Read(contents);
    value.compactMain=settingsstore::Integer(contents,"Serving","CompactMainDishes",0)!=0;
    value.compactDesserts=settingsstore::Integer(contents,"Serving","CompactDesserts",0)!=0;
    auto readTarget=[&](const char* key){
        const auto text=settingsstore::Get(contents,"Serving",key,"14");
        try {std::size_t consumed{};const auto result=std::stof(text,&consumed);return consumed==text.size()&&ValidDiameter(result)?result:14.f;}
        catch(...){return 14.f;}
    };
    value.targetDiameter=readTarget("TargetMainDiameter");value.targetDessertDiameter=readTarget("TargetDessertDiameter");
    std::lock_guard lock(mutex);settings=value;
    logger::info("Optional compact servings main={} main_target={} desserts={} dessert_target={}",value.compactMain,value.targetDiameter,value.compactDesserts,value.targetDessertDiameter);
}
Settings GetSettings(){std::lock_guard lock(mutex);return settings;}
bool SaveSettings(Settings value) {
    if(!Valid(value))return false;std::lock_guard lock(mutex);
    if(!settingsstore::SaveSection("Serving",{{"TargetMainDiameter",std::to_string(value.targetDiameter)},
        {"CompactMainDishes",value.compactMain?"1":"0"},{"TargetDessertDiameter",std::to_string(value.targetDessertDiameter)},
        {"CompactDesserts",value.compactDesserts?"1":"0"}})){
        logger::error("Serving size save/readback failed; live settings unchanged");return false;
    }
    settings=value;
    logger::info("Serving size saved main={} main_target={} desserts={} dessert_target={}",value.compactMain,value.targetDiameter,value.compactDesserts,value.targetDessertDiameter);
    return true;
}
bool InitializeSettingsPage() {
    if(initialized)return true;
    menuFramework=GetModuleHandleW(L"SKSEMenuFramework.dll");if(!menuFramework)return false;
    if(!settingsui::Available())return false;
    settingsui::Load();
    for(const auto name:{"AddSectionItem","igCombo_Str_arr","igTextWrappedV","igBeginDisabled","igEndDisabled"})
        if(!GetProcAddress(menuFramework,name)){logger::warn("Serving size page unavailable: missing {}",name);return false;}
    registeredSection=T("call_settings_section");registeredPage=T("serving_size_page");
    SKSEMenuFramework::SetSection(registeredSection);
    SKSEMenuFramework::AddSectionItem(registeredPage,Draw);initialized=true;return true;
}
bool RefreshSettingsPageLabel() {
    if(!initialized)return true;
    using Rename=bool (*)(const char*,const char*);
    const auto rename=reinterpret_cast<Rename>(GetProcAddress(menuFramework,"RenameSection"));
    if(!rename)return false;
    const std::string next=T("serving_size_page");
    if(registeredPage!=next) {
        const auto path=registeredSection+"/"+registeredPage;
        if(!rename(path.c_str(),next.c_str()))return false;
        registeredPage=next;
    }
    registeredSection=T("call_settings_section");return true;
}
}
