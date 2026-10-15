#include "PCH.h"
#include "TableDiscovery.h"
#include "Catalog.h"
#include <unordered_map>
namespace inn::discovery {
namespace {
std::unordered_map<RE::FormID,int> records;
std::unordered_map<std::string,int> models;
using Part=std::pair<int,std::uint32_t>;
std::unordered_map<RE::FormID,std::vector<Part>> embedded;
std::unordered_map<std::string,std::vector<Part>> embeddedModels;
std::mutex snapshotMutex;
RE::FormID snapshotCenter{};
std::vector<Surface> snapshot;
void Adopt(RE::TESBoundObject* base) {
    if(!base)return;
    const auto model=base->As<RE::TESModel>();const auto path=model?model->GetModel():nullptr;
    if(path)if(const auto parts=embeddedModels.find(TableModelKey(path));parts!=embeddedModels.end()) {
        embedded[base->GetFormID()]=parts->second;records[base->GetFormID()]=parts->second.front().first;return;
    }
    if(records.contains(base->GetFormID()))return;
    if(path)if(const auto i=models.find(TableModelKey(path));i!=models.end())records.emplace(base->GetFormID(),i->second);
}
}
void Initialize() {
    records.clear();models.clear();embedded.clear();embeddedModels.clear();auto data=RE::TESDataHandler::GetSingleton();
    for(const auto& alias:catalog.tableAliases) {
        if(!alias.model.empty())models.try_emplace(alias.model,alias.index);
        // TESBoundObject inherits FORMTYPE=None; resolve the actual record first.
        if(auto form=data->LookupForm(alias.local,alias.plugin))
            if(auto base=form->As<RE::TESBoundObject>())records.try_emplace(base->GetFormID(),alias.index);
    }
    for(const auto& part:catalog.embeddedAliases) {
        embeddedModels[part.model].push_back({part.index,part.segment});
        if(auto form=data->LookupForm(part.local,part.plugin))if(auto base=form->As<RE::TESBoundObject>()) {
            embedded[base->GetFormID()].push_back({part.index,part.segment});records.try_emplace(base->GetFormID(),part.index);
        }
    }
    // Arrays hold TESForm subobject pointers, not complete-object pointers.
    // The typed accessor reinterprets them; for MSTT its later base conversion
    // would add +0x20 a second time. Preserve the engine's TESForm address.
    for(const auto type:{RE::FormType::Static,RE::FormType::Furniture,RE::FormType::MovableStatic})
        for(auto form:data->GetFormArray(type))
            if(form)Adopt(form->As<RE::TESBoundObject>());
    logger::info("Table discovery profiles={} model_aliases={} loaded_base_records={}",catalog.tables.size(),models.size(),records.size());
    logger::info("Embedded tabletop regions={} loaded_composite_records={}",catalog.embeddedAliases.size(),embedded.size());
}
int Index(RE::TESBoundObject* base) {
    if(base)if(const auto row=records.find(base->GetFormID());row!=records.end())return row->second;
    return -1;
}
std::vector<Surface> Surfaces(RE::TESObjectREFR* center,float radius) {
    std::vector<Surface> result;auto cell=center?center->GetParentCell():nullptr;if(!cell)return result;
    const auto p=center->GetPosition();const Point point{p.x,p.y,p.z};
    cell->ForEachReference([&](RE::TESObjectREFR* ref){
        const int index=ref?Index(ref->GetBaseObject()):-1;
        if(index>=0&&!ref->IsDeleted()&&!ref->IsMarkedForDeletion()&&!ref->IsDisabled()&&ref->Is3DLoaded()&&ref->GetScale()>.1f&&std::abs(ref->GetAngleX())<.0175f&&std::abs(ref->GetAngleY())<.0175f) {
            const auto p=ref->GetPosition();
            auto append=[&](int profile,std::uint32_t segment) {
                Surface surface{ref->GetFormID(),profile,{p.x,p.y,p.z},ref->GetAngleZ(),ref->GetScale(),catalog.tables[profile],catalog.tableShapes[profile],catalog.tableCounters[profile],segment};
                if(radius<=0||surface.EdgeDistance(point)<=radius)result.push_back(surface);
            };
            if(const auto parts=embedded.find(ref->GetBaseObject()->GetFormID());parts!=embedded.end())
                for(const auto [profile,segment]:parts->second)append(profile,segment);
            else append(index,0);
        }
        return RE::BSContainer::ForEachResult::kContinue;
    });
    std::sort(result.begin(),result.end(),[&](const auto& a,const auto& b){
        const auto da=a.EdgeDistance(point),db=b.EdgeDistance(point);return da==db?std::tie(a.reference,a.segment)<std::tie(b.reference,b.segment):da<db;
    });
    return result;
}
std::vector<RE::TESObjectREFR*> Nearby(RE::TESObjectREFR* center,float radius) {
    auto surfaces=Surfaces(center,std::clamp(radius,100.f,600.f));std::vector<RE::TESObjectREFR*> result;
    std::vector<Surface> selected;
    for(const auto& surface:surfaces) {
        if(auto ref=RE::TESForm::LookupByID<RE::TESObjectREFR>(surface.reference)){result.push_back(ref);selected.push_back(surface);}
        if(result.size()==64)break;
    }
    {std::lock_guard lock(snapshotMutex);snapshotCenter=center?center->GetFormID():0;snapshot=std::move(selected);}
    logger::info("Nearby supported tables cell={:08X} candidates={} returned={}",center&&center->GetParentCell()?center->GetParentCell()->GetFormID():0,surfaces.size(),result.size());
    return result;
}
int ProfileAt(RE::TESObjectREFR* center,int slot) {
    std::lock_guard lock(snapshotMutex);
    return center&&center->GetFormID()==snapshotCenter&&slot>=0&&slot<static_cast<int>(snapshot.size())?snapshot[slot].profile:-1;
}
}
