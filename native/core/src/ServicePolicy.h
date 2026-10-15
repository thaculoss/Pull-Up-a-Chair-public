#pragma once
#include <cstdint>
#include <cmath>
namespace inn::service {

enum class Phase {preparing,approaching,talking,ordering,releasing};
inline bool RepeatBellAllowed(bool called,Phase phase){return called&&(phase==Phase::preparing||phase==Phase::approaching);}
inline constexpr float resetSettle=.25f,lostPackageGrace=.75f,stuckDelay=4.f;
inline constexpr unsigned recoveryLimit=2;
struct Motion {
    float x{},y{},z{},still{};
    void Reset(float a,float b,float c){x=a;y=b;z=c;still=0;}
    void Observe(float a,float b,float c,float dt){
        if((a-x)*(a-x)+(b-y)*(b-y)+(c-z)*(c-z)>=144.f)Reset(a,b,c);
        else still+=dt;
    }
};
inline bool NeedsRecovery(float stationary,float lost){return stationary>=stuckDelay||lost>=lostPackageGrace;}
inline bool MatchesOrder(std::uint32_t expected,std::uint32_t received){return expected!=0&&expected==received;}
inline bool BeforeOrder(Phase phase){return phase==Phase::preparing||phase==Phase::approaching||phase==Phase::talking;}
// Cleanup is eligible once per seated visit at ordering entry. A new called
// service may clear leftovers at its order entry; ringing alone never cleans.
struct CleanupVisit {
    std::uint32_t seat{},cell{},inn{};bool sealed{};
    bool Observe(std::uint32_t nextSeat,std::uint32_t nextCell,std::uint32_t nextInn){
        if(seat==nextSeat&&cell==nextCell&&inn==nextInn)return false;
        *this={nextSeat,nextCell,nextInn,false};return true;
    }
    bool Pending()const{return seat&&cell&&inn&&!sealed;}
    void Seal(){sealed=true;}
};
inline bool CleanupArmed(bool called,const CleanupVisit& visit){return called||visit.Pending();}
enum class CleanupEntry {conversation,arrival,orderBegin};
inline bool AutoCleanupAllowed(CleanupEntry entry,Phase phase,bool called,const CleanupVisit& visit,
                               bool enabled,bool cleaned,bool matchingContext){
    return entry==CleanupEntry::orderBegin&&BeforeOrder(phase)&&CleanupArmed(called,visit)&&enabled&&!cleaned&&matchingContext;
}
enum class Step { wait,arrive,cancel,failed,retry };
// Engine movement is separate from orders. Arrival never means food was bought.
struct Progress {bool context{},server{},paused{},atTable{},pathDone{},pathSuccess{};float elapsed{};unsigned retries=1;float outcomeAge{};bool chatting{};};
inline Step Decide(const Progress& p) {
    if(!p.context||!p.server)return Step::cancel;
    if(p.paused)return Step::wait;
    if(p.chatting)return p.elapsed>=35.f?Step::failed:Step::wait;
    if(p.atTable)return Step::arrive;
    if(p.elapsed>=35.0f)return Step::failed;
    if(p.pathDone&&!p.pathSuccess){
        if(p.retries==0)return p.outcomeAge>=.6f?Step::retry:Step::wait;
        return Step::failed;
    }
    // A completed path that stopped outside conversation distance is unusable.
    if(p.pathDone)return Step::failed;
    return Step::wait;
}
}
