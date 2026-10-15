// Compile the production Query/Begin/Finish/API implementation against a small
// engine boundary. These checks exercise actual core inventory/state mutations.
#include "DiningClaims.h"
#include "DiningInteraction.h"
#include "TablePolicy.h"
#include <algorithm>
#include <iostream>
#include <map>
#include <mutex>
#include <string_view>
#include <string>
#include <stdexcept>

namespace RE {
enum class ITEM_REMOVE_REASON { kRemove };
struct AlchemyItem {
    unsigned id{};
    unsigned GetFormID() const { return id; }
    template<class T> T* As() { return reinterpret_cast<T*>(this); }
};
struct TESObjectREFR {
    unsigned id{}; AlchemyItem* base{}; void* cell{}; bool exists{true};
    AlchemyItem* GetBaseObject() { return base; }
    void* GetParentCell() { return cell; }
};
inline AlchemyItem food{200};
struct TESForm {
    template<class T> static T* LookupByID(unsigned id) {
        return id == food.id ? reinterpret_cast<T*>(&food) : nullptr;
    }
};
struct PlayerCharacter {
    bool combat{}, seated{true}; void* cell{}; int count{4}, addDelta{1};
    static PlayerCharacter* GetSingleton();
    int GetItemCount(AlchemyItem*) { return count; }
    bool IsInCombat() { return combat; }
    void* GetParentCell() { return cell; }
    void AddObjectToContainer(AlchemyItem*, void*, int, void*) { count += addDelta; }
    void RemoveItem(AlchemyItem*, int quantity, ITEM_REMOVE_REASON, void*, void*) { count -= quantity; }
};
inline PlayerCharacter player;
PlayerCharacter* PlayerCharacter::GetSingleton() { return &player; }
struct SendHUDMessage { static void ShowHUDMessage(const char*) {} };
}
namespace logger {
template<class... Args> void info(const char*, Args&&...) {}
template<class... Args> void warn(const char*, Args&&...) {}
}
namespace inn::tables {
std::recursive_mutex mutex;
std::map<unsigned, Serving> servings;
dining::Claims claims;
std::map<unsigned, RE::TESObjectREFR> world;
int removed{}, updated{};
RE::TESObjectREFR* Ref(unsigned id) { auto it=world.find(id);return it==world.end()?nullptr:&it->second; }
bool Exists(RE::TESObjectREFR* ref) { return ref && ref->exists; }
bool IsSeated(RE::PlayerCharacter* player) { return player && player->seated; }
void Remove(RE::TESObjectREFR* ref) { if(Exists(ref)){ref->exists=false;++removed;} }
void UpdatePlate(unsigned) { ++updated; }
const char* T(const char*) { return "Eat"; }
#include "DiningCoreExcerpt.inl"
}
int checks{};
void Check(bool ok) { ++checks;if(!ok)throw std::runtime_error("Production core check "+std::to_string(checks)); }
void Reset() {
    using namespace inn::tables;
    claims.Clear(); servings.clear(); world.clear(); removed=updated=0;
    RE::player={}; RE::player.cell=&RE::food;
    for(unsigned id:{100u,101u}) {
        world[id]={id,&RE::food,RE::player.cell,true};
        inn::Serving row; row.reference=id;row.base=200;row.order=9;row.committed=1;
        servings[id]=row;
    }
    auto plate=servings[100];plate.reference=300;plate.kind=1;servings[300]=plate;
}
int main() {
    try {
        using namespace inn::tables;
        using namespace inn::dining;
        Reset();const auto api=DiningAPI(1);Check(Compatible(api));Check(!DiningAPI(2));
        ServingInfo info;Check(api->Query(100,&info));
        Check(info.reference==100&&info.base==200&&info.plate==300&&info.order==9&&info.committed);
        info.size=4;Check(!api->Query(100,&info));Check(!api->Query(999,&info));
        char label[2]{};Check(api->CopyEatLabel(label,2));Check(label[0]=='E'&&label[1]==0);
        Check(!api->CopyEatLabel(nullptr,1));
        Use use;
        servings[100].committed=0;Check(!api->Begin(100,&use));servings[100].committed=1;
        servings[100].kind=1;Check(!api->Begin(100,&use));servings[100].kind=0;
        RE::player.seated=false;Check(!api->Begin(100,&use));RE::player.seated=true;
        RE::player.combat=true;Check(!api->Begin(100,&use));RE::player.combat=false;
        world[100].cell=nullptr;Check(!api->Begin(100,&use));world[100].cell=RE::player.cell;
        servings[100].base=201;Check(!api->Begin(100,&use));servings[100].base=200;
        Check(RE::player.count==4&&servings.size()==3&&removed==0);
        Check(api->Begin(100,&use));Check(RE::player.count==5);
        Use second;Check(!api->Begin(100,&second));Check(!api->Begin(101,&second));
        Check(api->Finish(use.ticket,false));Check(RE::player.count==4&&Exists(Ref(100))&&servings.contains(100));
        Check(!api->Finish(use.ticket,true));Check(removed==0&&updated==0);
        Check(meal::UseManaged(api,100,[](auto& request) {
            Check(request.meal.food==100&&request.meal.plate==300);
            --RE::player.count;return true;
        })==meal::UseResult::succeeded);
        Check(RE::player.count==4&&!Exists(Ref(100))&&!servings.contains(100)&&servings.contains(101));
        Check(removed==1&&updated==1);
        Check(!api->Begin(100,&use));
        Check(meal::UseManaged(api,101,[](auto&){return false;})==meal::UseResult::failed);
        Check(RE::player.count==4&&Exists(Ref(101))&&removed==1&&updated==1);
        try { meal::UseManaged(api,101,[](auto&)->bool{throw std::runtime_error("native boundary");}); }
        catch(const std::runtime_error&) {}
        Check(RE::player.count==4&&servings.contains(101));
        Check(api->Begin(101,&use));claims.Clear();RE::player.count=4;
        Check(api->Begin(101,&second));Check(!api->Finish(use.ticket,true));
        Check(api->Finish(second.ticket,false));Check(RE::player.count==4);
        Reset();RE::player.addDelta=0;Check(!api->Begin(100,&use));
        Check(RE::player.count==4&&Exists(Ref(100)));RE::player.addDelta=1;
        Check(api->Begin(100,&use));Check(api->Finish(use.ticket,false));
        Reset();Check(api->Begin(100,&use));--RE::player.count;
        servings[100].order=10;Check(!api->Finish(use.ticket,true));
        Check(Exists(Ref(100))&&servings[100].order==10&&removed==0);
        std::cout<<checks<<" production core receipt checks passed\n";
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
