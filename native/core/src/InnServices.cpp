#include "PCH.h"
#include "InnServices.h"
#include "SettingsStore.h"
#include "Localization.h"
namespace inn {
namespace {

void ApplyTopicLanguage() {
    if(auto data=RE::TESDataHandler::GetSingleton()) {
        if(auto topic=data->LookupForm<RE::TESTopic>(0x807,"Pull up a chair Ordering.esp"))topic->fullName=T("order_topic");
        if(auto topic=data->LookupForm<RE::TESTopic>(0x80A,"Pull up a chair Ordering.esp"))topic->fullName=T("clean_topic");
        if(auto topic=data->LookupForm<RE::TESTopic>(0x80D,"Pull up a chair Ordering.esp"))topic->fullName=T("restore_topic");
    }
}
}
bool IsSeated(RE::PlayerCharacter* player) {
    return player && player->GetOccupiedFurniture() &&
        player->AsActorState()->GetSitSleepState()==RE::SIT_SLEEP_STATE::kIsSitting;
}
RE::BGSLocation* FindInn(RE::TESObjectREFR* reference) {
    auto keyword=RE::TESForm::LookupByID<RE::BGSKeyword>(0x1CB87);
    auto location=reference?reference->GetCurrentLocation():nullptr;
    for(int depth=0;location&&depth<12;++depth,location=location->parentLoc)
        if(keyword&&location->HasKeyword(keyword))return location;
    return nullptr;
}
void LoadLanguage() {
    std::string contents;settingsstore::Read(contents);
    std::string language=settingsstore::Get(contents,"General","Language","en");
    if(language=="auto") {
        auto settings=RE::INISettingCollection::GetSingleton();
        auto gameLanguage=settings?settings->GetSetting("sLanguage:General"):nullptr;
        language=gameLanguage&&gameLanguage->GetString()?gameLanguage->GetString():"english";
        std::transform(language.begin(),language.end(),language.begin(),[](unsigned char c){return char(std::tolower(c));});
        language=language.find("chinese")!=std::string::npos||language.find("taiwan")!=std::string::npos||language.starts_with("zh")?"zh":"en";
    }
    if(!strings.Load("Data/SKSE/Plugins/PullUpAChairOrdering/locales",language))logger::error("English language fallback missing");
    ApplyTopicLanguage();
    logger::info("Inn ordering language={}",language);
}
bool SaveLanguage(bool chinese) {
    if(!settingsstore::SaveSection("General",{{"Language",chinese?"zh":"en"}})){
        logger::error("Interface language save/readback failed; live language unchanged");return false;
    }
    strings.UseChinese(chinese);
    SKSE::GetTaskInterface()->AddTask(ApplyTopicLanguage);
    logger::info("Interface language saved={}; text applied immediately",chinese?"zh":"en");
    return true;
}
}
