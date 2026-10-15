#include "ServiceScenePolicy.h"
#include <iostream>
#include <stdexcept>
using namespace inn::service::scenes;
int checks{};
void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
int main(){try{
    StopRequest failed;
    Check(failed.Poll(0x67720,.1f)==StopStep::request,"First scene stop not requested");
    failed.Dispatched(false);
    Check(!failed.accepted,"Rejected dispatch marked as accepted");
    for(int i=0;i<4;++i)Check(failed.Poll(0x67720,.1f)==StopStep::retryWait,"Failure entered shutdown wait or spammed dispatch");
    Check(failed.Poll(0x67720,.11f)==StopStep::request,"Rejected scene never retried");
    failed.Dispatched(false);
    Check(failed.Poll(0x67720,.5f)==StopStep::request,"Third bounded attempt unavailable");
    failed.Dispatched(false);
    for(int i=0;i<50;++i)Check(failed.Poll(0x67720,.1f)==StopStep::dispatchFailed,"Rejected request retried forever or waited for nonexistent shutdown");
    Check(failed.attempts==3,"Dispatch retry cap changed");

    StopRequest accepted;
    Check(accepted.Poll(0x67720,0)==StopStep::request,"Fresh request unavailable");
    accepted.Dispatched(false);
    Check(accepted.Poll(0x67720,.5f)==StopStep::request,"Second request unavailable");
    accepted.Dispatched(true);
    for(int i=0;i<20;++i)Check(accepted.Poll(0x67720,0)==StopStep::shutdownWait,"Paused time consumed shutdown budget");
    for(int i=0;i<9;++i)Check(accepted.Poll(0x67720,.5f)==StopStep::shutdownWait,"Accepted stop retried or timed out prematurely");
    Check(accepted.Poll(0x67720,.5f)==StopStep::shutdownTimeout,"Accepted but stuck scene did not time out");
    Check(accepted.attempts==2,"Accepted stop was resent");

    Check(accepted.Poll(0xFE123AFC,.1f)==StopStep::request,"A new scene inherited the prior scene's accepted marker");
    Check(accepted.attempts==0&&!accepted.accepted,"New scene reused stale request state");
    accepted.Dispatched(true);
    accepted={}; // engine scene cleared / service cancelled / staff changed
    Check(accepted.scene==0&&accepted.attempts==0&&!accepted.accepted,"Cancellation retained pending scene state");
    Check(accepted.Poll(0xFE123AFC,0)==StopStep::request,"Later service could not stop the same scene again");

    for(const auto& row:chatty){
        Check(ChattyIdentity(row.scene,row.quest,row.sceneName,row.questName),"Reviewed Chatty pair missing");
        Check(!ChattyIdentity(row.scene,row.quest+1,row.sceneName,row.questName),"Reparented scene permitted");
        Check(!ChattyIdentity(row.scene,row.quest,"replacement",row.questName),"Patch builder accepted different scene identity");
    }
    Check(!ChattyIdentity(0x8BF,0x8B8,"JB1ChattyFollowersDO","JB1ChattyFollowers"),"Follower scenes became interruptible");
    Check(!ChattyIdentity(0xD0E,0xD0D,"JB1FollowerEngineDO","JB1FollowerEngine"),"Follower engine became interruptible");
    Check(!ChattyIdentity(0xF00,0xF01,"NewScene","NewQuest"),"Unknown Chatty scene became interruptible");
    std::cout<<checks<<" scene dispatch and compatibility checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
