#include "PCH.h"
#include "BarterPricing.h"
#include "BarterPolicy.h"
namespace inn {
float menuBarFraction=.12f,menuScale=1.f;
namespace {
float Setting(const char* key,float fallback) {
    auto settings=RE::GameSettingCollection::GetSingleton();auto setting=settings?settings->GetSetting(key):nullptr;
    return setting?setting->GetFloat():fallback;
}
}
int BuyCost(RE::TESBoundObject* object,RE::Actor* merchant) {
    auto player=RE::PlayerCharacter::GetSingleton();if(!object||!player||!merchant)return -1;
    auto values=player->AsActorValueOwner();if(!values)return -1;
    BarterInputs inputs{values->GetActorValue(RE::ActorValue::kSpeech),values->GetActorValue(RE::ActorValue::kSpeechcraftModifier),
        Setting("fBarterMin",2.f),Setting("fBarterMax",3.3f),Setting("fBarterBuyMin",1.05f)};
    auto factor=SpeechFactor(inputs);if(factor<0)return -1;
    if(player->HasPerkEntries(static_cast<RE::BGSEntryPointPerkEntry::EntryPoint>(RE::BGSEntryPoint::ENTRY_POINT::kModBuyPrices))) {
        // Verified against the installed game's barter call at SkyrimSE+0x85214C:
        // (entry point, perk owner, merchant, float* modifier), no item argument.
        RE::BGSEntryPoint::HandleEntryPoint(RE::BGSEntryPoint::ENTRY_POINT::kModBuyPrices,player,merchant,&factor);
        if(!std::isfinite(factor)||factor<0){logger::error("Invalid buy-price factor {}; form={:08X}",factor,object->GetFormID());return -1;}
    }
    return BuyPrice(std::max(0,object->GetGoldValue()),factor,inputs.minBuyFactor);
}
void LoadMenuLayoutSettings() {
    constexpr auto config=L".\\Data\\SKSE\\Plugins\\PullUpAChairOrdering.ini";
    int height=120;
    if(GetFileAttributesW(L".\\Data\\SKSE\\Plugins\\SD.ini")!=INVALID_FILE_ATTRIBUTES) {
        const int enabled=GetPrivateProfileIntW(L"Direction",L"bLetterbox",1,L".\\Data\\SKSE\\Plugins\\SD.ini");
        height=enabled?GetPrivateProfileIntW(L"Direction",L"iLetterboxHeight",120,L".\\Data\\SKSE\\Plugins\\SD.ini"):0;
        const int userEnabled=GetPrivateProfileIntW(L"Direction",L"bLetterbox",enabled,L".\\Data\\SKSE\\Plugins\\SD_user.ini");
        height=userEnabled?GetPrivateProfileIntW(L"Direction",L"iLetterboxHeight",height,L".\\Data\\SKSE\\Plugins\\SD_user.ini"):0;
    }
    height=GetPrivateProfileIntW(L"Menu",L"LetterboxHeight",height,config);
    menuBarFraction=std::clamp(height/1000.f,0.f,.4f);
    menuScale=std::clamp(GetPrivateProfileIntW(L"Menu",L"ScalePercent",100,config)/100.f,.5f,1.4f);
    logger::info("Menu safe area bars={} scale={}",menuBarFraction,menuScale);
}
}
