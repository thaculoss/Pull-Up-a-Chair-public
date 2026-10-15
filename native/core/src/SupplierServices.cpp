#include "PCH.h"
#include "SupplierServices.h"
#include "InnServices.h"
#include "Catalog.h"
#include <chrono>
#include <set>
namespace inn::suppliers {
namespace {
std::recursive_mutex mutex;
std::map<RE::FormID,std::vector<Identity>> registered,learned;
struct Role {RE::FormID quest{};std::uint32_t alias{};};
std::map<std::pair<RE::FormID,RE::FormID>,Role> roles;
std::map<RE::FormID,std::chrono::steady_clock::time_point> scanned;
std::map<RE::FormID,Identity> lastReported;
RE::BGSLocation* Inn(RE::BGSLocation* location) {
    auto keyword=RE::TESForm::LookupByID<RE::BGSKeyword>(0x1CB87);
    for(int depth=0;location&&depth<12;++depth,location=location->parentLoc)
        if(keyword&&location->HasKeyword(keyword))return location;
    return nullptr;
}
bool Exists(RE::TESObjectREFR* ref){return ref&&!ref->IsDeleted()&&!ref->IsMarkedForDeletion();}
RE::FormID RoleOwner(const Role& role) {
    auto quest=RE::TESForm::LookupByID<RE::TESQuest>(role.quest);
    auto owner=quest?quest->GetAliasedRef(role.alias).get():RE::NiPointer<RE::TESObjectREFR>{};
    return owner&&owner->As<RE::Actor>()?owner->GetFormID():0;
}
bool DurableAssociation(RE::FormID inn,RE::Actor* actor,RE::TESObjectREFR* stock) {
    if(!actor||!Exists(stock))return false;
    const auto home=Inn(actor->GetEditorLocation());
    auto stockHome=Inn(stock->GetEditorLocation());if(!stockHome)stockHome=FindInn(stock);
    return BelongsToInn(inn,home?home->GetFormID():0,stockHome?stockHome->GetFormID():0);
}
Candidate Inspect(Identity row) {
    auto actor=RE::TESForm::LookupByID<RE::Actor>(row.merchant);
    auto faction=RE::TESForm::LookupByID<RE::TESFaction>(row.faction);
    auto role=RE::TESForm::LookupByID<RE::TESFaction>(0x5091B);
    // VENC is read from the final faction record each time. Do not silently restore
    // the old chest when another mod intentionally changes or removes it.
    auto stock=faction&&faction->IsVendor()?faction->vendorData.merchantContainer:nullptr;
    row.stock=Exists(stock)?stock->GetFormID():0;
    bool affiliated=actor&&faction&&actor->IsInFaction(faction);
    if(auto bound=roles.find({row.inn,row.faction});bound!=roles.end()) {
        // Replacement owners inherit the dialogue alias, not necessarily the
        // old owner's vendor faction. The inn registry, not their other job,
        // remains the authority for the stock chest.
        affiliated=HasAuthority(true,RoleOwner(bound->second),row.merchant,affiliated);
    }
    if(auto known=registered.find(row.inn);known!=registered.end()) {
        affiliated=affiliated&&std::ranges::any_of(known->second,[&](const auto& canonical){return canonical.faction==row.faction;});
    }else{
        // Saved candidates are hints, not perpetual ownership. Recheck after a
        // script or overhaul changes the home location or faction's VENC.
        affiliated=affiliated&&DurableAssociation(row.inn,actor,stock);
    }
    return {row,Exists(actor)&&!actor->IsDead(),actor&&!actor->IsDisabled(),
        actor&&role&&actor->IsInFaction(role),faction&&faction->IsVendor()&&affiliated,row.stock!=0};
}
void Remember(const Identity& row) {
    if(!row.inn||!row.merchant||!row.faction||learned.size()>=512)return;
    auto& entries=learned[row.inn];
    if(entries.size()>=32)return;
    for(auto& existing:entries)if(existing.merchant==row.merchant&&existing.faction==row.faction){existing=row;return;}
    entries.push_back(row);
}
void Discover(RE::BGSLocation* location) {
    const auto inn=location->GetFormID();const auto now=std::chrono::steady_clock::now();
    if(auto t=scanned.find(inn);t!=scanned.end()&&now-t->second<std::chrono::seconds(15))return;
    scanned[inn]=now;
    std::set<RE::FormID> canonical;
    if(auto r=registered.find(inn);r!=registered.end())for(const auto& row:r->second)canonical.insert(row.faction);
    std::vector<RE::FormID> actors;
    {
        const auto [forms,lock]=RE::TESForm::GetAllForms();RE::BSReadLockGuard guard{lock};
        if(forms)for(const auto& [id,form]:*forms)if(form&&form->As<RE::Actor>())actors.push_back(id);
    }
    auto role=RE::TESForm::LookupByID<RE::TESFaction>(0x5091B);
    for(auto id:actors) {
        auto actor=RE::TESForm::LookupByID<RE::Actor>(id);
        if(!Exists(actor)||actor->IsDead()||actor->IsDisabled()||!role||!actor->IsInFaction(role))continue;
        if(!canonical.empty()) {
            // Alias-controlled owners are read directly in Resolve, including
            // custom successors that do not inherit the canonical vendor faction.
            for(auto faction:canonical) {
                if(roles.contains({inn,faction}))continue;
                auto f=RE::TESForm::LookupByID<RE::TESFaction>(faction);
                if(f&&actor->IsInFaction(f))Remember({inn,id,faction,0,100});
            }
            continue;
        }
        auto faction=actor->GetVendorFaction();
        if(!faction)faction=actor->GetVendorFaction(); // CommonLib's first call populates its cached field.
        auto stock=faction&&faction->IsVendor()?faction->vendorData.merchantContainer:nullptr;
        if(!Exists(stock))continue;
        // Never adopt an innkeeper merely visiting another inn. Require a durable
        // home/chest association before caching a third-party inn's provider.
        if(DurableAssociation(inn,actor,stock))
            Remember({inn,id,faction->GetFormID(),stock->GetFormID(),100});
    }
}
}
void Initialize() {
    std::lock_guard lock(mutex);registered.clear();roles.clear();Revert();
    auto data=RE::TESDataHandler::GetSingleton();
    try {
        Catalog::Read("Data/SKSE/Plugins/PullUpAChairOrdering/catalog/suppliers.tsv",[&](const auto& fields){
            if(fields.size()!=13)throw std::runtime_error("suppliers.tsv needs 13 fields");
            auto resolve=[&](int column){return data->LookupForm(std::stoul(fields[column+1],nullptr,16),fields[column]);};
            auto a=resolve(0),b=resolve(2),c=resolve(4),d=resolve(6);
            auto location=a?a->As<RE::BGSLocation>():nullptr;
            auto actor=b?b->As<RE::Actor>():nullptr;
            auto faction=c?c->As<RE::TESFaction>():nullptr;
            auto chest=d?d->As<RE::TESObjectREFR>():nullptr;
            if(!location||!faction)return; // An optional plugin can be absent.
            auto canonical=Inn(location);if(!canonical)return;
            registered[canonical->GetFormID()].push_back({canonical->GetFormID(),actor?actor->GetFormID():0,faction->GetFormID(),chest?chest->GetFormID():0,static_cast<unsigned>(std::stoul(fields[8]))});
            if(fields[9]!="-") {
                auto form=data->LookupForm(std::stoul(fields[10],nullptr,16),fields[9]);
                auto quest=form?form->As<RE::TESQuest>():nullptr;
                // Keep an unresolved role bound and unavailable, rather than
                // treating arbitrary faction members as the current owner.
                roles[{canonical->GetFormID(),faction->GetFormID()}]={quest?quest->GetFormID():0,static_cast<unsigned>(std::stoul(fields[11]))};
            }
        });
    }catch(const std::exception& e){logger::error("Supplier registry: {}",e.what());}
    logger::info("Supplier registry loaded {} inn mappings; merchant presence in the dining room is not required",registered.size());
}
Service Resolve(RE::Actor* server) {
    auto location=FindInn(server);if(!location)return {};
    std::lock_guard lock(mutex);const auto inn=location->GetFormID();
    auto inspect=[&]{
        std::vector<Candidate> candidates;
        for(const auto& [key,role]:roles)if(key.first==inn)
            if(auto owner=RoleOwner(role))candidates.push_back(Inspect({inn,owner,key.second,0,0}));
        for(auto map:{&registered,&learned})if(auto rows=map->find(inn);rows!=map->end())
            for(auto row:rows->second)candidates.push_back(Inspect(row));
        return candidates;
    };
    auto candidates=inspect();auto best=Select(inn,candidates);
    if(!best){Discover(location);candidates=inspect();best=Select(inn,candidates);}
    if(!best)return {};
    const auto selected=best->id;
    if(!lastReported.contains(inn)||lastReported.at(inn)!=selected) {
        lastReported[inn]=selected;
        logger::info("Inn supplier resolved inn={:08X} merchant={:08X} faction={:08X} stock={:08X} source={}",
            inn,selected.merchant,selected.faction,selected.stock,selected.priority<100?"registered":"learned");
    }
    return {selected,RE::TESForm::LookupByID<RE::Actor>(selected.merchant),RE::TESForm::LookupByID<RE::TESObjectREFR>(selected.stock)};
}
void Save(SKSE::SerializationInterface* io) {
    std::lock_guard lock(mutex);
    for(const auto& [inn,rows]:learned)for(const auto& row:rows)io->WriteRecord(recordType,1,row);
}
void Load(SKSE::SerializationInterface* io,std::uint32_t version,std::uint32_t length) {
    if(version!=1||length!=sizeof(Identity))return;
    Identity row{};if(io->ReadRecordData(row)!=sizeof(row))return;
    for(auto field:{&row.inn,&row.merchant,&row.faction})if(*field) {
        std::uint32_t resolved{};if(!io->ResolveFormID(*field,resolved))return;*field=resolved;
    }
    row.stock=0; // Recomputed from the current canonical faction, never a saved chest.
    row.priority=100;std::lock_guard lock(mutex);Remember(row);
}
void Revert(){std::lock_guard lock(mutex);learned.clear();scanned.clear();lastReported.clear();}
}
