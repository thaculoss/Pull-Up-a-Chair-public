#include "PCH.h"
#include "Supply.h"
namespace innservice {
namespace {
std::map<RE::FormID,Role> extraRoles;
std::map<RE::FormID,RE::FormID> bindings;
Role ActorRole(RE::Actor* actor) {
    if(auto base=actor->GetActorBase())if(auto i=extraRoles.find(base->GetFormID());i!=extraRoles.end())return i->second;
    auto owner=RE::TESForm::LookupByID<RE::TESFaction>(0x5091B);auto waiter=RE::TESForm::LookupByID<RE::TESFaction>(0xDEE93);
    if(owner&&actor->IsInFaction(owner))return Role::owner;
    if(waiter&&actor->IsInFaction(waiter))return Role::waiter;
    return Role::none;
}
bool FoodVendor(RE::TESFaction* faction) {
    const auto list=faction->vendorData.vendorSellBuyList;
    if(!list||faction->vendorData.vendorValues.notBuySell)return false;
    return list->HasForm(RE::TESForm::LookupByID(0x8CDEA));
}
RE::TESFaction* StockFaction(RE::Actor* actor,Role role,RE::BGSLocation* inn) {
    auto current=actor->GetVendorFaction();
    if(current&&current->IsVendor()&&current->vendorData.merchantContainer)return current;
    // Vendor business-hour conditions must not erase the inn's stock identity.
    // Base memberships are rechecked against the actor's current faction state.
    RE::TESFaction* found=nullptr;
    auto base=actor->GetActorBase();if(!base)return nullptr;
    for(const auto& entry:base->factions) {
        auto faction=entry.faction;auto chest=faction&&faction->IsVendor()?faction->vendorData.merchantContainer:nullptr;
        if(!chest||!actor->IsInFaction(faction)||!FoodVendor(faction)||(role!=Role::owner&&FindInn(chest)!=inn))continue;
        if(!found||faction->GetFormID()<found->GetFormID())found=faction;
    }
    return found;
}
}
RE::BGSLocation* FindInn(RE::TESObjectREFR* ref) {
    auto location=ref?ref->GetCurrentLocation():nullptr;auto keyword=RE::TESForm::LookupByID<RE::BGSKeyword>(0x1CB87);
    std::set<RE::FormID> visited;
    for(int n=0;location&&n<16;++n,location=location->parentLoc) {
        if(!visited.insert(location->GetFormID()).second)break;
        if(keyword&&location->HasKeyword(keyword))return location;
    }
    return nullptr;
}
bool Seated(RE::PlayerCharacter* player) {
    return player&&player->GetOccupiedFurniture()&&player->AsActorState()->GetSitSleepState()==RE::SIT_SLEEP_STATE::kIsSitting;
}
bool Available(RE::Actor* actor,RE::PlayerCharacter* player) {
    if(!actor||!player||actor==player||actor->IsDeleted()||actor->IsDisabled()||actor->IsDead()||!actor->Is3DLoaded()||actor->IsInCombat()||actor->IsPlayerTeammate()||actor->GetCurrentScene()||actor->IsHostileToActor(player))return false;
    const auto state=actor->AsActorState()->GetSitSleepState();
    return state!=RE::SIT_SLEEP_STATE::kIsSleeping&&state!=RE::SIT_SLEEP_STATE::kWantToSleep;
}
Resolved Resolve(RE::TESObjectREFR* reference) {
    Resolved result;auto location=FindInn(reference);auto cell=reference?reference->GetParentCell():nullptr;
    if(!location||!cell)return result;
    const auto inn=location->GetFormID();auto player=RE::PlayerCharacter::GetSingleton();std::vector<Vendor> vendors;
    cell->ForEachReference([&](RE::TESObjectREFR* ref){
        auto actor=ref?ref->As<RE::Actor>():nullptr;
        if(actor&&actor!=player&&FindInn(actor)==location) {
            const auto role=ActorRole(actor);
            if(role!=Role::none) {
                const auto faction=StockFaction(actor,role,location);
                auto chest=faction&&faction->IsVendor()?faction->vendorData.merchantContainer:nullptr;
                // An innkeeper's own faction may deliberately hold its chest offsite.
                // A waiter's unrelated vendor faction must still belong to this inn.
                if(chest&&role!=Role::owner&&FindInn(chest)!=location)chest=nullptr;
                const auto a=actor->GetPosition(),p=reference->GetPosition();
                result.staff.push_back({actor->GetFormID(),actor->GetActorBase()?actor->GetActorBase()->GetFormID():0,inn,cell->GetFormID(),faction?faction->GetFormID():0,chest?chest->GetFormID():0,role,std::hypot(a.x-p.x,a.y-p.y),Available(actor,player)});
                if(chest&&!actor->IsDead()&&!actor->IsDisabled()&&!actor->IsDeleted())vendors.push_back({inn,faction->GetFormID(),chest->GetFormID(),actor->GetFormID(),FoodVendor(faction),role==Role::owner});
            }
        }
        return RE::BSContainer::ForEachResult::kContinue;
    });
    // Persistent faction stock remains usable when an innkeeper is in another room or asleep.
    for(auto faction:RE::TESDataHandler::GetSingleton()->GetFormArray<RE::TESFaction>()) {
        auto chest=faction&&faction->IsVendor()?faction->vendorData.merchantContainer:nullptr;
        if(chest&&FindInn(chest)==location&&FoodVendor(faction))vendors.push_back({inn,faction->GetFormID(),chest->GetFormID(),0,true,false});
    }
    if(auto binding=bindings.find(inn);binding!=bindings.end()) {
        auto faction=RE::TESForm::LookupByID<RE::TESFaction>(binding->second);
        auto chest=faction&&faction->IsVendor()?faction->vendorData.merchantContainer:nullptr;
        if(chest) {
            RE::FormID merchant=0;
            for(const auto& member:result.staff)if(member.faction==faction->GetFormID()&&(!merchant||member.id<merchant))merchant=member.id;
            result.supply=ChooseSupply(inn,{{inn,faction->GetFormID(),chest->GetFormID(),merchant,true,true}},static_cast<std::uint32_t>(result.staff.size()));
        }else {result.supply.inn=inn;result.supply.staffCount=static_cast<std::uint32_t>(result.staff.size());}
    }else result.supply=ChooseSupply(inn,vendors,static_cast<std::uint32_t>(result.staff.size()));
    return result;
}
void LoadRoles() {
    extraRoles.clear();std::ifstream file("Data/SKSE/Plugins/PullUpAChairService/roles.tsv");std::string line;
    while(std::getline(file,line))if(!line.empty()&&line[0]!='#') {
        auto first=line.find('\t'),last=line.find('\t',first+1);if(first==std::string::npos||last==std::string::npos)continue;
        try {
            auto role=line.substr(last+1);if(!role.empty()&&role.back()=='\r')role.pop_back();
            if(role!="waiter"&&role!="owner")continue;
            auto form=RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESNPC>(std::stoul(line.substr(first+1,last-first-1),nullptr,16),line.substr(0,first));
            if(form)extraRoles[form->GetFormID()]=role=="waiter"?Role::waiter:Role::owner;
        }catch(const std::exception& e){logger::warn("Ignored invalid staff role row: {}",e.what());}
    }
    logger::info("Additional registered staff bases={}",extraRoles.size());
    bindings.clear();std::ifstream sources("Data/SKSE/Plugins/PullUpAChairService/bindings.tsv");
    while(std::getline(sources,line))if(!line.empty()&&line[0]!='#') {
        std::vector<std::string> parts;std::istringstream row(line);std::string field;
        while(std::getline(row,field,'\t'))parts.push_back(field);
        if(parts.size()!=4)continue;if(!parts.back().empty()&&parts.back().back()=='\r')parts.back().pop_back();
        try {
            auto inn=RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSLocation>(std::stoul(parts[1],nullptr,16),parts[0]);
            auto faction=RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESFaction>(std::stoul(parts[3],nullptr,16),parts[2]);
            if(inn&&faction)bindings[inn->GetFormID()]=faction->GetFormID();
        }catch(const std::exception& e){logger::warn("Ignored invalid stock binding: {}",e.what());}
    }
    logger::info("Explicit inn stock bindings={}",bindings.size());
}
}
