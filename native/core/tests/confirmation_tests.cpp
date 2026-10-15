#include "Policy.h"
#include "OrderPolicy.h"
#include <functional>
#include <mutex>
#include <vector>
#include <iostream>
#include <stdexcept>

int checks{};
void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
bool gameThread{},context=true,hasPlayer=true,hasGold=true;
int goldReads{},balance=4,generation=1;
namespace RE {
struct TESBoundObject{};
TESBoundObject gold;
struct TESForm {template<class T>static T* LookupByID(unsigned){return hasGold?&gold:nullptr;}};
struct PlayerCharacter {
    static PlayerCharacter* GetSingleton(){static PlayerCharacter player;return hasPlayer?&player:nullptr;}
    int GetItemCount(TESBoundObject*){Check(gameThread,"Gold was read from the UI thread");++goldReads;return balance;}
};
}
namespace SKSE {
struct TaskInterface {std::vector<std::function<void()>> queue;void AddTask(std::function<void()> task){queue.push_back(std::move(task));}} tasks;
TaskInterface* GetTaskInterface(){return &tasks;}
}
namespace inn::service {int Generation(){return generation;}}
namespace logger {template<class... A>void info(const char*,A&&...) {}}
std::string T(const char* key){return key;}
struct Session {
    food::Phase phase{food::Phase::open};bool confirmationQueued{};int token=10;
    std::array<inn::Id,3> selected{0,0,0x64B2F};std::string status;
} session;
struct Window {bool IsOpen=true;} testWindow;
Window* window=&testWindow;
std::mutex gate;
inn::Quote quote;
inn::Quote CurrentQuote(){return quote;}
bool SameContext(){Check(gameThread,"Context was read from the UI thread");return context;}
void CancelUnlocked(){session.phase=food::Phase::cancelled;window->IsOpen=false;}
// This file is generated directly from the production source at configure time.
// The test drives the actual request callback, not a copy of its algorithm.
#include "OrderConfirmationExcerpt.inl"

void Reset(){session=Session{};testWindow=Window{};window=&testWindow;gameThread=false;context=hasPlayer=hasGold=true;goldReads=0;balance=4;generation=1;SKSE::tasks.queue.clear();quote={};quote.valid=true;quote.total=9;}
void Request(){std::lock_guard lock(gate);RequestConfirmationUnlocked();}
void Run(){auto tasks=std::move(SKSE::tasks.queue);SKSE::tasks.queue.clear();gameThread=true;for(auto& task:tasks)task();gameThread=false;}
int main(){try {
    Reset();Request();Request();
    Check(SKSE::tasks.queue.size()==1&&session.confirmationQueued,"Repeated confirmation queued duplicate work");
    Check(goldReads==0&&session.phase==food::Phase::open&&window->IsOpen,"UI confirmation touched world inventory or started serving");
    Run();
    Check(session.phase==food::Phase::open&&window->IsOpen&&!session.confirmationQueued&&session.status=="gold_missing","4 gold allowed a 9-gold apple to spawn or closed the menu");
    Check(balance==4&&goldReads==1,"Rejected confirmation changed money or repeated inventory reads");
    balance=9;Request();Run();
    Check(session.phase==food::Phase::selected&&!window->IsOpen&&balance==9,"Exact funds did not begin serving without charging early");
    Reset();balance=149;Request();Run();
    Check(session.phase==food::Phase::selected&&!window->IsOpen&&balance==149,"Successful diagnostic apple scenario rejected or charged before serving");
    Reset();balance=149;Request();balance=4;Run();
    Check(session.phase==food::Phase::open&&session.status=="gold_missing","Confirmation used stale gold from the UI frame");
    Reset();Request();session.phase=food::Phase::cancelled;window->IsOpen=false;Run();
    Check(session.phase==food::Phase::cancelled&&!window->IsOpen&&goldReads==0,"Queued confirmation revived a cancelled order");
    Reset();Request();session.token=11;session.confirmationQueued=true;Run();
    Check(session.token==11&&session.phase==food::Phase::open&&session.confirmationQueued&&goldReads==0,"Old request mutated a new order");
    Reset();Request();++generation;Run();
    Check(session.phase==food::Phase::open&&goldReads==0,"Pre-load callback submitted an order after generation changed");
    Reset();Request();session.selected[2]=0x64B2E;Run();
    Check(session.phase==food::Phase::open&&!session.confirmationQueued&&goldReads==0,"Changed food selection was submitted by an old click");
    Reset();context=false;Request();Run();
    Check(session.phase==food::Phase::cancelled&&!window->IsOpen&&goldReads==0,"Lost seat context allowed serving");
    Reset();quote.valid=false;Request();Run();
    Check(session.phase==food::Phase::open&&window->IsOpen&&session.status=="stock_changed"&&goldReads==0,"Invalid quote allowed serving");
    Reset();quote.total=0;balance=0;Request();Run();
    Check(session.phase==food::Phase::selected&&!window->IsOpen,"Valid free meal was rejected");
    std::cout<<checks<<" actual confirmation callback checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
