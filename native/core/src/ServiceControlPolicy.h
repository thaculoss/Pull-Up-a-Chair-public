#pragma once
#include "ServicePolicy.h"
#include <cstdint>
namespace inn::service::control {
enum class Slot { empty,owned,engineDialogue,engineTransition,external,innRoutine };
struct Package {bool present{},owned{},created{},hasFile{},hasQuest{};std::uint32_t id{},type{},procedure{};bool knownInnRoutine{};};
inline Slot Classify(const Package& package){
    if(!package.present)return Slot::empty;
    if(package.owned)return Slot::owned;
    if(package.knownInnRoutine)return Slot::innRoutine;
    const bool generated=!package.hasQuest&&((package.id&0xFF000000)==0xFF000000||
        (!package.hasFile&&(package.created||package.id==0)));
    if(generated){
        if(package.type==15||package.type==28)return Slot::engineDialogue;
        if(package.type==26||package.type==31||package.type==36)return Slot::engineTransition;
        if(package.type==18){
            if(package.procedure==10||package.procedure==13||package.procedure==33)return Slot::engineDialogue;
            if(package.procedure==19||package.procedure==34||package.procedure==44)return Slot::engineTransition;
        }
    }
    return Slot::external;
}
enum class Action { keep,install,interruptDialogue,waitTransition,protectedControl };
inline Action Decide(Slot slot,bool playerDialogue,bool protectedScene){
    if(playerDialogue||protectedScene)return Action::protectedControl;
    switch(slot){
        case Slot::empty:return Action::install;
        case Slot::owned:return Action::keep;
        case Slot::engineDialogue:case Slot::innRoutine:return Action::interruptDialogue;
        case Slot::engineTransition:return Action::waitTransition;
        default:return Action::protectedControl;
    }
}
inline constexpr float transitionGrace=5.f,protectedGrace=3.f;
inline constexpr unsigned resumeLimit=3;
inline bool WaitExpired(Action action,float elapsed){
    return elapsed>=(action==Action::waitTransition?transitionGrace:protectedGrace);
}
enum class Recovery { queuedRecheck,reinstall,handoff };
inline Recovery ChooseRecovery(unsigned attempts,bool owned,bool stalled){
    if(attempts>=recoveryLimit)return Recovery::handoff;
    if(attempts==0&&owned&&stalled)return Recovery::queuedRecheck;
    return Recovery::reinstall;
}
}
