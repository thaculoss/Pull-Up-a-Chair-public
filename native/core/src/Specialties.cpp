#include "PCH.h"
#include "Specialties.h"
#include "Catalog.h"
#include "Localization.h"
namespace inn::specialties {
namespace {std::map<RE::FormID,std::array<RE::FormID,3>> menus;}
void Load(const std::filesystem::path& folder) {
    menus.clear();auto data=RE::TESDataHandler::GetSingleton();
    for(const auto& profile:catalog.foods)if(profile.plugin=="pull up a chair ordering.esp") {
        auto food=data->LookupForm<RE::AlchemyItem>(profile.local,profile.plugin);
        if(food)food->fullName=T(fmt::format("specialty_food_{:06X}",profile.local));
    }
    Catalog::Read(folder/"specialties.tsv",[&](const auto& fields){
        if(fields.size()!=6)throw std::runtime_error("specialties.tsv needs 6 fields");
        auto location=data->LookupForm<RE::BGSLocation>(static_cast<std::uint32_t>(std::stoul(fields[1],nullptr,16)),fields[0]);
        std::array<RE::FormID,3> forms{};
        for(int i=0;i<3;++i) {
            const auto local=static_cast<std::uint32_t>(std::stoul(fields[i+3],nullptr,16));
            auto food=data->LookupForm<RE::AlchemyItem>(local,"Pull up a chair Ordering.esp");
            if(!food)throw std::runtime_error("Missing independent specialty food "+fields[i+3]);
            forms[i]=food->GetFormID();
        }
        if(location)menus[location->GetFormID()]=forms;
    });
    logger::info("House specialties loaded: {} inns, independent generated food source",menus.size());
}
std::array<RE::FormID,3> ForInn(RE::BGSLocation* location) {
    for(int depth=0;location&&depth<12;++depth,location=location->parentLoc)
        if(auto i=menus.find(location->GetFormID());i!=menus.end())return i->second;
    return {};
}
}
