#include "PCH.h"
#include "DailyMenus.h"
#include "Catalog.h"
#include "Policy.h"
#include <tuple>
namespace inn::daily {
namespace {
struct Daily {RE::FormID inn{};std::uint32_t day{};std::array<RE::FormID,3> foods{};};
static_assert(sizeof(Daily)==20);
std::map<std::pair<RE::FormID,std::uint32_t>,Daily> menus;
std::mutex mutex;
}
std::uint32_t Today() {
    auto calendar=RE::Calendar::GetSingleton();
    return calendar?CalendarDay(calendar->GetYear(),calendar->GetMonth(),static_cast<unsigned>(calendar->GetDay())):0;
}
std::array<Id,3> Menu(RE::BGSLocation* location,std::uint32_t day,const std::map<Id,Offer>& offers) {
    if(!location||!day||offers.empty())return {};
    std::lock_guard lock(mutex);const auto key=std::pair(location->GetFormID(),day);
    if(auto existing=menus.find(key);existing!=menus.end()) {
        const auto& foods=existing->second.foods;const std::array<Id,3> dishes{foods[0],foods[1],foods[2]};
        // Stock depletion must not redraw a daily menu. Check the full category
        // catalog, including foods currently absent from the merchant chest.
        auto categories=offers;auto data=RE::TESDataHandler::GetSingleton();
        for(const auto& profile:catalog.foods)if(profile.plugin!="pull up a chair ordering.esp")
            if(auto form=data->LookupForm<RE::AlchemyItem>(profile.local,profile.plugin))
                if(profile.MatchesModel(form->GetModel()?form->GetModel():""))categories.try_emplace(form->GetFormID(),Offer{0,0,profile.mask});
        if(DailyCategoriesMatch(dishes,categories))return dishes;
        menus.erase(existing);
        logger::info("Daily menu categories changed inn={:08X} day={}; rebuilding once",location->GetFormID(),day);
    }
    auto file=location->GetFile(0);
    const auto innKey=food::Hash(file?file->fileName:"Skyrim.esm")^location->GetLocalFormID();
    auto result=PickDaily(innKey,day,offers);
    if(Price(result,offers).valid) {
        if(menus.size()>=4096)menus.erase(menus.begin());
        menus[key]={location->GetFormID(),day,{BaseForm(result[0]),BaseForm(result[1]),BaseForm(result[2])}};
        logger::info("Daily menu inn={:08X} day={} foods={:08X},{:08X},{:08X}",location->GetFormID(),day,result[0],result[1],result[2]);
    }
    return result;
}
void Save(SKSE::SerializationInterface* io) {
    std::lock_guard lock(mutex);for(const auto& [key,row]:menus)io->WriteRecord(recordType,1,row);
}
void Load(SKSE::SerializationInterface* io,std::uint32_t version,std::uint32_t length) {
    if(version!=1||length!=sizeof(Daily))return;
    Daily row{};if(io->ReadRecordData(row)!=sizeof(row))return;
    std::uint32_t id{};if(!io->ResolveFormID(row.inn,id))return;row.inn=id;
    for(auto& food:row.foods)if(food){if(!io->ResolveFormID(food,id))return;food=id;}
    std::lock_guard lock(mutex);if(menus.size()<4096)menus[{row.inn,row.day}]=row;
}
void Revert(){std::lock_guard lock(mutex);menus.clear();}
}
