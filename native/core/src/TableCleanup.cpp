#include "PCH.h"
#include "TableCleanup.h"
#include "TableManager.h"
#include "SurfacePolicy.h"
#include "Catalog.h"
#include "InnServices.h"
#include "Localization.h"
#include "TableDiscovery.h"
#include "CallService.h"
#include "CleanupPolicy.h"
#include <set>
namespace inn::cleanup {
namespace {
struct Cleared {RE::FormID reference{},base{},cell{},table{};std::uint32_t segment{};};
static_assert(sizeof(Cleared)==20);
std::map<RE::FormID,Cleared> removed;
std::mutex mutex;
std::set<std::string> edibleModels;
std::map<std::string,float> foodModelBottoms;
std::string ModelKey(std::string_view value) {
    auto key=TableModelKey(value);
    if(key.starts_with("_pgpatcher_dups\\")) {
        const auto separator=key.find('\\',16);if(separator!=std::string::npos)key.erase(0,separator+1);
    }
    return key;
}
void RefreshFoodModels() {
    edibleModels.clear();foodModelBottoms.clear();
    auto data=RE::TESDataHandler::GetSingleton();
    for(const auto type:{RE::FormType::AlchemyItem,RE::FormType::Ingredient})for(auto form:data->GetFormArray(type)) {
        auto base=form?form->As<RE::TESBoundObject>():nullptr;if(!base)continue;
        const auto food=base->As<RE::AlchemyItem>();
        const auto ingredient=base->As<RE::IngredientItem>();
        const bool edible=(food&&(food->IsFood()||food->HasKeywordString("VendorItemFood")||food->HasKeywordString("VendorItemFoodRaw")))||
            (ingredient&&(ingredient->HasKeywordString("VendorItemFood")||ingredient->HasKeywordString("VendorItemFoodRaw")));
        auto model=base->As<RE::TESModel>();
        if(edible&&model&&model->GetModel())edibleModels.insert(ModelKey(model->GetModel()));
    }
    for(const auto& profile:catalog.foods) {
        auto form=data->LookupForm(profile.local,profile.plugin);auto model=form?form->As<RE::TESModel>():nullptr;
        if(model&&model->GetModel())foodModelBottoms.try_emplace(ModelKey(model->GetModel()),profile.bounds[2]);
        if(!profile.expectedModel.empty())foodModelBottoms.try_emplace(ModelKey(profile.expectedModel),profile.bounds[2]);
    }
}
Point Position(RE::TESObjectREFR* ref){const auto p=ref->GetPosition();return {p.x,p.y,p.z};}
bool Exists(RE::TESObjectREFR* ref){return ref&&!ref->IsDeleted()&&!ref->IsMarkedForDeletion();}
std::vector<Surface> Surfaces(RE::PlayerCharacter* player) {
    return discovery::Surfaces(player);
}
std::vector<Surface> CurrentTable(RE::PlayerCharacter* player,const std::vector<Surface>& surfaces,bool nearest=false) {
    auto seat=player->GetOccupiedFurniture().get();if(!seat)return {};
    const auto point=Position(player);const auto facing=player->GetAngleZ();
    const Surface* anchor=nullptr;float best=1e9f,second=1e9f;
    for(const auto& surface:surfaces) {
        const auto distance=surface.EdgeDistance(point);
        const auto height=surface.Top()-seat->GetPositionZ();
        const auto p=surface.Local(point);
        const float x=std::clamp(p.x,surface.bounds[0],surface.bounds[2]),y=std::clamp(p.y,surface.bounds[1],surface.bounds[3]);
        const float targetX=surface.origin.x+surface.scale*(x*std::cos(surface.yaw)+y*std::sin(surface.yaw));
        const float targetY=surface.origin.y+surface.scale*(-x*std::sin(surface.yaw)+y*std::cos(surface.yaw));
        const float forward=(targetX-point.x)*std::sin(facing)+(targetY-point.y)*std::cos(facing);
        if(distance>140||height<catalog.MinimumSeatHeight(surface.profile)||height>105||(!nearest&&forward<-10))continue;
        const auto score=distance+(nearest?0.f:std::max(0.f,-forward)*2);
        if(score<best||(score==best&&anchor&&surface.reference<anchor->reference)){second=best;anchor=&surface;best=score;}
        else second=std::min(second,score);
    }
    if(!anchor){return nearest?std::vector<Surface>{}:CurrentTable(player,surfaces,true);}
    // A turn toward the waiter must not choose a different table. Only use an
    // unambiguous nearest fallback within the original distance/height limits.
    if(nearest&&second-best<12.f)return {};
    using Key=std::pair<RE::FormID,std::uint32_t>;
    std::vector<Surface> result{*anchor};std::set<Key> selected{{anchor->reference,anchor->segment}};
    for(std::size_t i=0;i<result.size();++i)for(const auto& candidate:surfaces)
        if(!selected.contains({candidate.reference,candidate.segment})&&ConnectedCounters(result[i],candidate)){selected.insert({candidate.reference,candidate.segment});result.push_back(candidate);}
    return result;
}
bool FoodOrTableware(RE::TESBoundObject* base) {
    if(!base)return false;
    auto model=base->As<RE::TESModel>();auto path=model?model->GetModel():nullptr;
    if(base->Is(RE::FormType::Weapon)) {
        // Vanilla dining cutlery is WEAP; general daggers and quest weapons are excluded.
        return path&&(base->GetFormID()==0x1F25A||base->GetFormID()==0x1F25B)&&DiningWeaponModel(path);
    }
    if(auto food=base->As<RE::AlchemyItem>();food&&(food->IsFood()||food->HasKeywordString("VendorItemFood")||food->HasKeywordString("VendorItemFoodRaw")))return true;
    if(auto ingredient=base->As<RE::IngredientItem>();ingredient&&ingredient->HasKeywordString("VendorItemFood"))return true;
    if(!base->Is(RE::FormType::Misc,RE::FormType::Static,RE::FormType::MovableStatic,RE::FormType::AlchemyItem))return false;
    if(!path)return false;
    if(TablewareModel(path)||UtensilModel(path)||BeverageBottleModel(path)||FoodDisplayModel(path))return true;
    // Static food displays that use the same model as an actual edible item.
    return edibleModels.contains(ModelKey(path));
}
bool CleanupCandidate(RE::TESObjectREFR* ref,bool allItems) {
    auto base=ref?ref->GetBaseObject():nullptr;if(!base)return false;
    auto model=base->As<RE::TESModel>();const auto path=model?model->GetModel():nullptr;
    const bool fixedType=base->Is(RE::FormType::Static,RE::FormType::MovableStatic,RE::FormType::Container,RE::FormType::Activator,RE::FormType::Light);
    const bool fixture=discovery::Index(base)>=0||(fixedType&&path&&FixedStructureModel(path))||(!allItems&&path&&BulkContainerModel(path));
    const bool tabletop=base->Is(RE::FormType::Misc,RE::FormType::Static,RE::FormType::MovableStatic,
        RE::FormType::AlchemyItem,RE::FormType::Ingredient,RE::FormType::Weapon,RE::FormType::Armor,
        RE::FormType::Book,RE::FormType::Scroll,RE::FormType::SoulGem,RE::FormType::KeyMaster,
        RE::FormType::Ammo,RE::FormType::Container,RE::FormType::Light,RE::FormType::Activator);
    return Eligible(allItems,FoodOrTableware(base),tabletop,fixture);
}
float Bottom(RE::TESObjectREFR* ref) {
    const auto base=ref->GetBaseObject();
    for(const auto& profile:catalog.foods) {
        auto form=RE::TESDataHandler::GetSingleton()->LookupForm(profile.local,profile.plugin);
        if(form==base)return profile.bounds[2];
    }
    if(auto model=base->As<RE::TESModel>();model&&model->GetModel())
        if(const auto found=foodModelBottoms.find(ModelKey(model->GetModel()));found!=foodModelBottoms.end())return found->second;
    // Runtime local reference bounds cover unprofiled third-party food/tableware.
    return ref->GetBoundMin().z;
}
bool Here(RE::TESObjectREFR* ref,const std::vector<Surface>& table) {
    for(const auto& surface:table)if(OnSurface(surface,Position(ref),Bottom(ref),ref->GetScale()))return true;
    return false;
}
}
Target Capture() {
    Target target;auto player=RE::PlayerCharacter::GetSingleton();
    if(!IsSeated(player)||!player->GetParentCell())return target;
    auto seat=player->GetOccupiedFurniture().get();if(!seat)return target;
    target.seat=seat->GetFormID();target.cell=player->GetParentCell()->GetFormID();
    for(const auto& surface:CurrentTable(player,Surfaces(player)))target.regions.emplace_back(surface.reference,surface.segment);
    logger::info("Service table captured seat={:08X} cell={:08X} regions={}",target.seat,target.cell,target.regions.size());
    return target;
}
bool ExecuteNow(RE::Actor* server,bool restore,bool notify,const Target* target) {
        auto player=RE::PlayerCharacter::GetSingleton();
        if(!server||!IsSeated(player)||player->IsInCombat()||server->GetParentCell()!=player->GetParentCell()||!FindInn(server)||FindInn(server)!=FindInn(player))return false;
        auto surfaces=Surfaces(player);std::vector<Surface> table;
        if(target&&!target->regions.empty()){
            auto seat=player->GetOccupiedFurniture().get();
            if(!seat||seat->GetFormID()!=target->seat||player->GetParentCell()->GetFormID()!=target->cell)return false;
            for(const auto& key:target->regions){
                const auto i=std::find_if(surfaces.begin(),surfaces.end(),[&](const auto& surface){return surface.reference==key.first&&surface.segment==key.second;});
                if(i==surfaces.end()){logger::warn("Service captured table unavailable ref={:08X} segment={}",key.first,key.second);return false;}
                table.push_back(*i);
            }
        }else table=CurrentTable(player,surfaces);
        if(table.empty()){logger::warn("Table cleanup skipped: no unambiguous table seat={:08X} candidates={} captured={}",target?target->seat:0,surfaces.size(),target?target->regions.size():0);if(notify)RE::SendHUDMessage::ShowHUDMessage(T("no_table"));return false;}
        using Key=std::pair<RE::FormID,std::uint32_t>;
        std::set<Key> ids;for(const auto& surface:table)ids.insert({surface.reference,surface.segment});
        const auto cell=player->GetParentCell();int count=0,drinkwareCount=0,utensilCount=0,bottleCount=0;
        if(restore) {
            std::lock_guard lock(mutex);
            for(auto i=removed.begin();i!=removed.end();) {
                auto ref=RE::TESForm::LookupByID<RE::TESObjectREFR>(i->first);
                const bool legacyRegion=i->second.segment==0&&ref&&Here(ref,table)&&std::any_of(table.begin(),table.end(),[&](const auto& surface){return surface.reference==i->second.table;});
                if(i->second.cell==cell->GetFormID()&&(ids.contains({i->second.table,i->second.segment})||legacyRegion)) {
                    if(Exists(ref)&&ref->IsDisabled()&&ref->GetParentCell()==cell&&ref->GetBaseObject()->GetFormID()==i->second.base){ref->Enable(false);++count;}
                    i=removed.erase(i);
                }else ++i;
            }
            if(notify)RE::SendHUDMessage::ShowHUDMessage(T(count?"table_restored":"nothing_to_restore"));return true;
        }
        RefreshFoodModels();
        const bool allItems=call::GetSettings().clearAllTableItems;
        std::vector<RE::ObjectRefHandle> candidates;
        unsigned bottleAudits=0;
        cell->ForEachReference([&](RE::TESObjectREFR* ref){
            if(!Exists(ref)||!ref->GetBaseObject())return RE::BSContainer::ForEachResult::kContinue;
            const bool managed=tables::Managed(ref->GetFormID());
            const bool available=(!ref->IsDisabled()||managed)&&(ref->Is3DLoaded()||managed);
            const bool eligible=CleanupCandidate(ref,allItems);
            const bool here=available&&eligible&&Here(ref,table);
            if(available&&eligible&&here)candidates.push_back(ref->GetHandle());
            const auto model=ref->GetBaseObject()->As<RE::TESModel>();
            if(bottleAudits<16&&model&&model->GetModel()&&BeverageBottleModel(model->GetModel())) {
                const auto position=Position(ref);
                const auto nearby=std::find_if(table.begin(),table.end(),[&](const auto& surface){return surface.EdgeDistance(position)<=48.f&&std::abs(position.z-surface.Top())<=120.f;});
                if(nearby!=table.end()) {
                    ++bottleAudits;
                    logger::info("Table cleanup bottle ref={:08X} base={:08X} model={} decision={} loaded={} disabled={} managed={} position=({:.2f},{:.2f},{:.2f}) bottom={:.2f} scale={:.3f} table_top={:.2f} edge_distance={:.2f}",ref->GetFormID(),ref->GetBaseObject()->GetFormID(),model->GetModel(),!available?"unavailable":!eligible?"excluded_type_or_fixture":!here?"outside_table_surface":"candidate",ref->Is3DLoaded(),ref->IsDisabled(),managed,position.x,position.y,position.z,Bottom(ref),ref->GetScale(),nearby->Top(),nearby->EdgeDistance(position));
                }
            }
            return RE::BSContainer::ForEachResult::kContinue;
        });
        for(const auto candidate:candidates)if(auto ref=candidate.get();Exists(ref.get())&&ref->GetBaseObject()) {
            const auto model=ref->GetBaseObject()->As<RE::TESModel>();
            const bool drinkware=model&&model->GetModel()&&DrinkwareModel(model->GetModel());
            const bool utensil=model&&model->GetModel()&&UtensilModel(model->GetModel());
            const bool bottle=model&&model->GetModel()&&BeverageBottleModel(model->GetModel());
            const bool managed=tables::Managed(ref->GetFormID());
            if(managed) {
                tables::Discard(ref->GetFormID());++count;if(drinkware)++drinkwareCount;if(utensil)++utensilCount;if(bottle)++bottleCount;
            }else {
                std::lock_guard lock(mutex);
                if(removed.size()>=8192){if(notify)RE::SendHUDMessage::ShowHUDMessage(T("cleanup_full"));break;}
                auto support=std::find_if(table.begin(),table.end(),[&](const auto& surface){return OnSurface(surface,Position(ref.get()),Bottom(ref.get()),ref->GetScale());});
                if(support==table.end())continue;
                removed[ref->GetFormID()]={ref->GetFormID(),ref->GetBaseObject()->GetFormID(),cell->GetFormID(),support->reference,support->segment};
                ref->Disable();++count;if(drinkware)++drinkwareCount;if(utensil)++utensilCount;if(bottle)++bottleCount;
            }
            if(bottle&&bottleCount<=16)logger::info("Table cleanup bottle action={} ref={:08X} base={:08X} disabled_after={}",managed?"discard_managed":"disable_foreign",ref->GetFormID(),ref->GetBaseObject()->GetFormID(),ref->IsDisabled());
        }
        logger::info("Whole-table cleanup anchor={:08X} segment={} profile={} surfaces={} scope={} cleared={} drinkware={} utensils={} bottles={} item_protection=off (includes foreign references)",table[0].reference,table[0].segment,table[0].profile,table.size(),allItems?"all_tabletop_items":"food_and_tableware",count,drinkwareCount,utensilCount,bottleCount);
        if(notify)RE::SendHUDMessage::ShowHUDMessage(T(count?"table_cleared":"nothing_to_clear"));
    return true;
}
void Request(RE::Actor* server,bool restore) {
    const auto handle=server?server->GetHandle():RE::ObjectRefHandle{};
    SKSE::GetTaskInterface()->AddTask([handle,restore]{auto ref=handle.get();ExecuteNow(ref?ref->As<RE::Actor>():nullptr,restore,true);});
}
void Save(SKSE::SerializationInterface* io){std::lock_guard lock(mutex);for(const auto& [id,row]:removed)io->WriteRecord(recordType,2,row);}
void Load(SKSE::SerializationInterface* io,std::uint32_t version,std::uint32_t length) {
    if(!((version==1&&length==16)||(version==2&&length==sizeof(Cleared))))return;
    Cleared row{};if(io->ReadRecordData(&row,length)!=length)return;
    for(auto field:{&row.reference,&row.base,&row.cell,&row.table}){std::uint32_t id{};if(!io->ResolveFormID(*field,id))return;*field=id;}
    std::lock_guard lock(mutex);if(removed.size()<8192)removed[row.reference]=row;
}
void Revert(){std::lock_guard lock(mutex);removed.clear();}
}
