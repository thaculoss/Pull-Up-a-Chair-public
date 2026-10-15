// Exercise production removal, save/load and recovery code at a fake engine
// boundary. The regression fixture deliberately loses live removal flags.
#include "DiningClaims.h"
#include "ServingRetirement.h"
#include "TablePolicy.h"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
namespace RE {
using FormID = std::uint32_t;
struct Form { FormID id{}; FormID GetFormID() { return id; } };
struct TESForm { struct ChangeFlags { static constexpr unsigned kFlags=1; }; };
struct TESObjectREFR : Form {
  Form base{}, cell{}; bool deleted{}, disabled{}; unsigned changes{};
  Form* GetBaseObject() { return &base; }
  Form* GetParentCell() { return cell.id?&cell:nullptr; }
  void Disable() { disabled=true;changes|=1; }
  void SetDelete(bool yes) { deleted=yes;changes=0; } // observed failure model
  void AddChange(unsigned flags) { changes|=flags; }
  void SetActivationBlocked(bool) {}
};
}
namespace SKSE {
struct SerializationInterface {
  struct Record { unsigned type,version; std::vector<unsigned char> data; };
  std::vector<Record> records;std::size_t cursor{};int active{-1};
  std::map<unsigned,unsigned> remap;
  template<class T> void WriteRecord(unsigned type,unsigned version,const T& value) {
    const auto p=reinterpret_cast<const unsigned char*>(&value);
    records.push_back({type,version,{p,p+sizeof(value)}});
  }
  bool GetNextRecordInfo(unsigned& type,unsigned& version,unsigned& length) {
    if(cursor==records.size())return false;
    active=static_cast<int>(cursor++);const auto& row=records[active];
    type=row.type;version=row.version;length=static_cast<unsigned>(row.data.size());return true;
  }
  template<class T> unsigned ReadRecordData(T& out) {
    if(records[active].data.size()!=sizeof(out))return 0;
    std::memcpy(&out,records[active].data.data(),sizeof(out));return sizeof(out);
  }
  bool ResolveFormID(unsigned before,unsigned& after) {
    if(auto i=remap.find(before);i!=remap.end()){after=i->second;return after!=0;}
    after=before;return true;
  }
};
}
namespace logger {
template<class... T> void info(const char*,T&&...) {}
}
namespace inn {
namespace cleanup { constexpr unsigned recordType=1;void Revert(){}void Load(SKSE::SerializationInterface*,unsigned,unsigned){} }
namespace daily { constexpr unsigned recordType=2;void Revert(){}void Load(SKSE::SerializationInterface*,unsigned,unsigned){} }
namespace suppliers { constexpr unsigned recordType=3;void Revert(){}void Load(SKSE::SerializationInterface*,unsigned,unsigned){} }
namespace service { constexpr unsigned recordType=4;void PrepareLoad(){}void Load(SKSE::SerializationInterface*,unsigned,unsigned){} }
namespace tables {
std::recursive_mutex mutex;
std::map<RE::FormID,Serving> servings;
std::map<RE::FormID,RetiredServing> retired;
std::map<RE::FormID,RE::TESObjectREFR> world;
dining::Claims claims;std::atomic_uint64_t notificationEpoch{1};
bool recovering{};unsigned orderSequence{};constexpr unsigned recordType=0x494F5442;
RE::TESObjectREFR* Ref(unsigned id) { auto i=world.find(id);return i==world.end()?nullptr:&i->second; }
bool Exists(RE::TESObjectREFR* ref) { return ref&&!ref->deleted; }
void UpdatePlate(unsigned) {}
#include "RetirementRemovalExcerpt.inl"
#include "RetirementSaveExcerpt.inl"
#include "RetirementLoadExcerpt.inl"
}
}
int checks{};
void Check(bool ok) { ++checks;if(!ok)throw std::runtime_error("Retirement check "+std::to_string(checks)); }
int main() {
  try {
    using namespace inn;using namespace inn::tables;
    constexpr unsigned id=0xFF0026B4,base=0xFE5BB80F,cell=0x1605E;
    Serving row;row.reference=id;row.base=base;row.cell=cell;row.order=9;row.committed=1;
    servings[id]=row;world[id]={{id},{base},{cell}};
    Remove(Ref(id));servings.erase(id);
    Check(retired.contains(id)&&Ref(id)->disabled&&Ref(id)->deleted&&Ref(id)->changes==1);
    SKSE::SerializationInterface saved;Save(&saved);
    Check(saved.records.size()==1&&saved.records[0].type==retiredServingRecord);
    Check(saved.records[0].version==1&&saved.records[0].data.size()==sizeof(RetiredServing));
    // Created initial data survived without deletion flags, exactly as Save41.
    world[id]={{id},{base},{cell}};Load(&saved);
    Check(servings.empty()&&retired.contains(id)&&recovering);
    Recover();Check(Ref(id)->deleted&&Ref(id)->disabled&&servings.empty());
    // No new serving / charge / inventory operation is involved in recovery.
    Check(!recovering&&retired.size()==1);
    SKSE::SerializationInterface again;Save(&again);Check(again.records.size()==1);
    world[id]={{id},{base},{cell}};Load(&again);Recover();Check(Ref(id)->deleted);
    // A recycled ID with a different base or cell must never be removed.
    for(auto replacement: {RE::TESObjectREFR{{id},{base+1},{cell}},RE::TESObjectREFR{{id},{base},{cell+1}}}) {
      again.cursor=0;Load(&again);world[id]=replacement;Recover();
      Check(!Ref(id)->deleted&&!retired.contains(id));
    }
    Check(!ValidRetirement({0xC8FC8,base,cell}));
    Check(!MatchesRetirement({id,base,cell},id+1,base,cell));
    Check(!MatchesRetirement({id,base,cell},id,base,0));
    // Unattached references survive save and recover when their cell attaches.
    again.cursor=0;Load(&again);world.clear();Recover();
    Check(retired.contains(id));SKSE::SerializationInterface pending;Save(&pending);
    Check(pending.records.size()==1);world[id]={{id},{base},{cell}};
    RecoverRetired(cell+1);Check(!Ref(id)->deleted);
    RecoverRetired(cell);Check(Ref(id)->deleted);
    // Paid, unconsumed food remains a paid serving across the same load.
    Revert();servings[id]=row;world[id]={{id},{base},{cell}};
    SKSE::SerializationInterface paid;Save(&paid);Check(paid.records.size()==1&&paid.records[0].type==recordType);
    Load(&paid);Recover();Check(servings.contains(id)&&!Ref(id)->deleted&&retired.empty());
    // A failed form remap cannot retire an unrelated record from a new order.
    again.cursor=0;again.remap[base]=0;Load(&again);Check(retired.empty());
    // World reset drops receipts from the previous save completely.
    Revert();Check(retired.empty()&&servings.empty());
    std::cout<<checks<<" production retirement save/load checks passed\n";
  } catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
