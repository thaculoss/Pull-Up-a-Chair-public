#include "ServiceControlPolicy.h"
#include "InnScenePolicy.h"
#include <iostream>
#include <stdexcept>
using namespace inn::service;
namespace {unsigned checks{};void Check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}}
int main(){try{
    using namespace control;
    // Ordinary schedules and BardSongs aliases no longer reserve an empty
    // run-once slot or revoke an existing service invitation.
    Check(Decide(Slot::empty,false,false)==Action::install,"Empty staff slot cannot start called service");
    Check(Decide(Slot::owned,false,false)==Action::keep,"Ordinary aliases revoke an existing invitation");

    // Captured 19:33:09 and 19:34:39 packages. Type 26/procedure 19 is
    // GetUpFromChairBed (TESPackage.h); its target is an inn furniture marker.
    for(const auto id:{0xFF0009D7u,0xFF002324u}){
        Package gettingUp{true,false,true,false,false,id,26,19};
        Check(Classify(gettingUp)==Slot::engineTransition,"Captured chair exit was mistaken for NPC chatter");
        Check(Decide(Classify(gettingUp),false,false)==Action::waitTransition,"Chair exit was forcibly interrupted");
        Check(!WaitExpired(Action::waitTransition,1.45f),"Captured chair exit times out before completing");
        Check(WaitExpired(Action::waitTransition,5.f),"Chair exit can block the invitation forever");
    }

    // 19:33:21: actual NPC conversation is type 28 with scene 6771C.
    Package conversation{true,false,true,false,false,0xFF000CCE,28,10};
    Check(scenes::Allowed(0x6771C,0x6771B),"Captured Ivarstead chatter cannot use the reviewed scene stop path");
    Check(Decide(Classify(conversation),false,true)==Action::protectedControl,"Scene package was cleared before Scene.Stop completed");
    Check(Decide(Classify(conversation),false,false)==Action::interruptDialogue,"Residual NPC dialogue cannot yield after scene shutdown");
    Check(Decide(Slot::empty,false,false)==Action::install,"Cleared scene cannot resume the same invitation");
    Check(Decide(Slot::owned,false,false)==Action::keep,"Resumed Travel unnecessarily resets the invitation");

    for(const auto slot:{Slot::empty,Slot::owned,Slot::engineDialogue,Slot::engineTransition,Slot::innRoutine}){
        Check(Decide(slot,true,false)==Action::protectedControl,"Rollback interrupts an actual player conversation");
        Check(Decide(slot,false,true)==Action::protectedControl,"Rollback overwrites a currently playing scene");
    }
    Package foreign{true,false,false,true,true,0x12000800,18,46};
    Check(Decide(Classify(foreign),false,false)==Action::protectedControl,"Rollback steals an authored run-once package");
    conversation.hasQuest=true;
    Check(Decide(Classify(conversation),false,false)==Action::protectedControl,"Unknown quest-owned dialogue was mistaken for engine chatter");
    std::cout<<checks<<" recorded service-control rollback checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
