#include "CallPolicy.h"
#include "CallFocusPolicy.h"
#include "ServicePolicy.h"
#include "ServiceSelection.h"
#include "ServiceControlPolicy.h"
#include "InnScenePolicy.h"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
int checks{};
void Check(bool value,const char* what){++checks;if(!value)throw std::runtime_error(what);}
int main(){try{
    {
        using namespace inn::call;
        using namespace inn::call::focus;
        const Settings dak{Mode::framework,0};
        // Captured 1.7.104 case: no camera table or crosshair reference,
        // effective DAK held, seated in an inn; Activate already calls staff.
        Context context{true,true,false,false,false,SafeCallTarget({})};
        auto canDispatch=Trigger(dak,276,true,true,true,true,context);
        Check(canDispatch,"Logged empty-crosshair call stopped working");
        Check(ChoosePrompt(false,true,false,canDispatch)==PromptKind::hidden,"Standard HUD option changed empty-crosshair presentation");
        Check(ChoosePrompt(true,true,false,canDispatch)==PromptKind::compatible,"Working DAK call still had no compatibility hint");
        Check(ChoosePrompt(false,true,true,canDispatch)==PromptKind::table,"Standard tabletop HUD was replaced");
        // The presentation option cannot turn an interactive item into a call.
        for(const TargetContext target:std::array{TargetContext{true,true,false,false,false},TargetContext{true,false,true,true,true},TargetContext{true,true,false,true,false}}){
            context.safeTarget=SafeCallTarget(target);
            canDispatch=Trigger(dak,276,true,true,true,true,context);
            Check(!canDispatch&&ChoosePrompt(true,true,false,canDispatch)==PromptKind::hidden,"Food/NPC/foreign furniture gained a misleading call hint");
        }
        context.safeTarget=true;
        for(unsigned state=0;state<64;++state){
            Context c{bool(state&1),bool(state&2),bool(state&4),bool(state&8),bool(state&16),bool(state&32)};
            const bool allowed=Trigger(dak,276,true,true,true,true,c);
            Check((ChoosePrompt(true,Enabled(dak,true,true,c),false,allowed)==PromptKind::compatible)==allowed,"Compatibility HUD disagreed with actual call eligibility");
        }
        Check(ChoosePrompt(true,false,true,true)==PromptKind::hidden,"Released DAK or another input mode retained compatibility text");
        PromptPublication pub;
        Check(pub.Request("[Gamepad A] Call staff",10,1),"Compatibility hint was not published");
        Check(pub.Request("",11,1),"Quick modifier release failed to hide fallback text");
        const auto removed=pub.generation;
        Check(pub.Request("[E] Call staff",12,removed),"Remapped keyboard hint could not reappear after controller use");
        Check(pub.Request("",13,removed),"Opening a blocking menu failed to clear fallback text");
    }
    {
        using inn::call::focus::PromptPublication;
        PromptPublication prompt;std::uint64_t hidden=1;
        Check(!prompt.Request("",0,hidden),"Idle HUD queued unnecessary empty updates");
        Check(prompt.Request("[Gamepad A] Call service",10,hidden),"Fresh seated hold did not publish");
        const auto shown=prompt.generation;
        Check(!prompt.Request(prompt.text,100,hidden),"Unchanged prompt bypassed layout refresh throttle");
        Check(prompt.Request("",110,hidden),"Menu opening did not immediately request removal");
        const auto clearing=prompt.generation;
        Check(clearing!=shown,"Queued show remained current after menu hide");
        // HUD unavailable during dialogue/menu: no UI acknowledgement arrives.
        Check(!prompt.Request("",200,hidden),"Pending removal retried on every game frame");
        Check(prompt.Request("",360,hidden),"Missed UI removal was cached as complete");
        Check(prompt.Request("",610,hidden),"Second failed removal stopped retrying");
        hidden=shown;
        Check(prompt.Request("",860,hidden),"Stale UI acknowledgement cancelled current removal");
        // Menu closes and the modifier is released while the player stays seated.
        Check(prompt.Request("",1110,hidden),"Post-order removal needed eating or standing to retry");
        hidden=clearing;
        Check(!prompt.Request("",1360,hidden),"Confirmed removal continued queueing UI work");
        Check(!prompt.Request("",5000,hidden),"Confirmed hidden prompt resumed idle polling");
        Check(prompt.Request("[Gamepad A] Call service",5010,hidden),"Repeat ordering required standing first");
        const auto secondShow=prompt.generation;
        Check(secondShow!=clearing,"New hold reused a cleared generation");
        Check(prompt.Request("",5020,hidden),"Quick release did not immediately queue removal");
        const auto secondClear=prompt.generation;
        Check(prompt.Request("[Gamepad A] Call service",5030,hidden),"Rapid repress was lost while removal pending");
        hidden=secondClear;
        Check(prompt.Request(prompt.text,5280,hidden),"Old removal acknowledgement suppressed a newer show");
        prompt.Invalidate();const auto reset=prompt.generation;
        Check(prompt.Request("",5281,hidden)&&prompt.generation>reset,"Reset did not invalidate and remove a visible field");
        hidden=prompt.generation;
        Check(!prompt.Request("",5600,hidden),"Reset removal acknowledgement ignored");
        prompt.Invalidate();
        Check(prompt.Request("",5601,hidden),"Reset of cached-hidden HUD did not verify field absence");
        Check(prompt.Request("",1,hidden),"Clock reset left pending removal permanently throttled");
    }
    {
        using namespace inn;using namespace inn::call::focus;
        const inn::call::Settings dak{inn::call::Mode::framework,0};
        const inn::call::Context sitting{true,true,false,false,false,true};
        Check(Enabled(dak,true,true,sitting),"Seated DAK hold did not enable table focus");
        Check(!Enabled(dak,true,false,sitting),"Call hint stayed visible after releasing DAK");
        auto standing=sitting;standing.seated=false;
        Check(!Enabled(dak,true,true,standing),"Standing player received table call hint");
        auto dialogue=sitting;dialogue.menuOpen=true;
        Check(!Enabled(dak,true,true,dialogue),"Call hint covered dialogue/menu interaction");
        auto combat=sitting;combat.inCombat=true;
        Check(!Enabled(dak,true,true,combat),"Combat retained table focus");
        Check(!Enabled({inn::call::Mode::hotkey,278},true,true,sitting),"Dedicated hotkey mode gained a DAK overlay");
        const Surface table{71,0,{0,0,0},0,1,{-60,-40,60,40,70}};
        const Point seat{0,-70,0};
        auto hit=PickTable(std::array{table},seat,{0,-90,130},{0,1,-1});
        Check(hit&&hit->reference==71&&std::abs(hit->point.y+30)<.01f,"Third-person center ray missed the nearby tabletop");
        Check(PickTable(std::array{table},seat,{0,-60,105},{0,1,-1}).has_value(),"First-person tabletop ray regressed");
        Check(!PickTable(std::array{table},seat,{90,-90,130},{0,1,-1}),"Off-table aim captured another BTPS target");
        Check(!PickTable(std::array{table},seat,{0,0,20},{0,0,1}),"Table underside exposed a call prompt");
        Check(!PickTable(std::array{table},seat,{0,0,130},{0,1,0}),"Parallel ray became an infinite target");
        Check(!PickTable(std::array{table},seat,{0,0,130},{0,0,0}),"Zero camera ray was accepted");
        Check(!PickTable(std::array{table},{0,-500,0},{0,0,130},{0,0,-1}),"Distant table became a seated call target");
        auto round=table;round.shape=SurfaceShape::ellipse;
        Check(!PickTable(std::array{round},seat,{59,39,130},{0,0,-1}),"Round table bounding-box corner stole a target");
        auto higher=table;higher.reference=72;higher.origin.z=10;
        Check(PickTable(std::array{table,higher},seat,{0,0,150},{0,0,-1})->reference==72,"Ray selected through the nearest surface");
        Check(hit&&Unobstructed(*hit,false,0,0),"Composite table without direct collision lost its geometry target");
        Check(hit&&Unobstructed(*hit,true,71,hit->distance),"Direct table collision rejected");
        Check(hit&&!Unobstructed(*hit,true,80,hit->distance-5),"Food on the table lost its original interaction");
        Check(hit&&!Unobstructed(*hit,true,0,hit->distance-30),"Wall without a reference failed to block focus");
        Check(hit&&!Unobstructed(*hit,true,71,hit->distance-40),"Composite wall was mistaken for its embedded table");
        Check(Fresh(1,2,1,2,1000,1050),"Fresh table focus rejected");
        Check(!Fresh(1,2,1,2,1000,1151),"Stale HUD target stole Activate");
        Check(!Fresh(1,2,3,2,1000,1050),"Seat change reused old call target");
        Check(!Fresh(1,2,1,3,1000,1050),"Cell change reused old call target");
    }
    {
        using namespace inn::call;
        Check(SafeCallTarget({}),"Empty tabletop view rejected");
        Check(SafeCallTarget({true,true,false,true,true}),"Ordinary static table rejected");
        Check(SafeCallTarget({true,true,true,true,false}),"Occupied table/bench furniture leaked Activate to Stand Up");
        Check(SafeCallTarget({true,true,true,false,false}),"BTPS-selected own chair could not call for its nearby table");
        Check(!SafeCallTarget({true,true,false,true,false}),"Another interactive table/bench lost Activate");
        Check(!SafeCallTarget({true,false,true,true,true}),"Stale selection from another cell was adopted");
        Check(!SafeCallTarget({true,true,false,false,false}),"Unrelated interactive selection lost Activate");
        ModifierState modifier;
        modifier.Observe(268,268,true);
        Check(modifier.Active(false),"Gamepad DAK press waited for delayed Papyrus global");
        modifier.Observe(268,276,true);
        Check(modifier.Active(false),"Activate event erased held D-pad modifier");
        modifier.Observe(268,268,false);
        Check(!modifier.Active(true),"Released modifier used stale Papyrus active state");
        modifier.Observe(42,18,true);
        Check(!modifier.Active(false),"Rebinding inherited an old held key");
        modifier.Observe(42,42,true);
        Check(modifier.Active(false),"Keyboard modifier did not work");
        modifier.Observe(42,42,false);
        Check(!modifier.Active(true),"Released keyboard modifier remained active");
        modifier={};modifier.Observe(268,276,true);
        Check(modifier.Active(true),"Script fallback lost on load before observing a modifier edge");
        modifier.Observe(0,276,true);
        Check(!modifier.Active(true),"Invalid DAK binding became active");
        for(const unsigned binding:{42u,256u,268u,280u,281u}){
            modifier={};modifier.Observe(binding,binding,true);
            const Settings dak{Mode::framework,0};Context sitting{true,true,false,false,false,true};
            Check(focus::Enabled(dak,true,modifier.Active(false),sitting),"Fresh hold failed before ordering");
            sitting.menuOpen=true;
            Check(!focus::Enabled(dak,true,modifier.Active(true),sitting),"Ordering menu retained table prompt");
            // Deliberately omit Observe(...,false): SMF can consume the release.
            modifier.Synchronize(binding,false);sitting.menuOpen=false;
            Check(!focus::Enabled(dak,true,modifier.Active(true),sitting),"Missed release left prompt visible until standing");
            Check(!ReserveFrameworkPress(dak,true,true,modifier.Active(true),sitting),"Missed modifier release kept stealing ordinary Activate");
            modifier.Synchronize(binding,true);
            Check(focus::Enabled(dak,true,modifier.Active(false),sitting),"Next real hold failed without standing first");
            modifier.Synchronize(binding,false);
            Check(!modifier.Active(true),"Second release revived stale script state");
        }
        modifier.Observe(268,268,true);modifier.Synchronize(42,false);
        Check(!modifier.Active(true)&&modifier.binding==42,"Live rebind retained old held key");
        modifier.Synchronize(0,true);Check(!modifier.Active(true),"Disabled binding gained a sampled hold");
        modifier.Synchronize(282,true);Check(!modifier.Active(true),"Out-of-range sampled binding became active");
        Settings dak{Mode::framework,0};Context seated{true,true,false,false,true,true};
        Check(ReserveFrameworkPress(dak,true,true,true,seated),"Busy service chord stood player up");
        Check(!Trigger(dak,276,true,true,true,true,seated),"Busy chord started a duplicate request");
        Check(!ReserveFrameworkPress(dak,true,true,false,seated),"Plain Activate could no longer leave the seat");
        Check(!ReserveFrameworkPress(dak,true,false,true,seated),"Missing DAK stole an Activate press");
        for(int field=0;field<5;++field){auto c=seated;switch(field){case 0:c.seated=false;break;case 1:c.inInn=false;break;case 2:c.inCombat=true;break;case 3:c.menuOpen=true;break;case 4:c.safeTarget=false;break;}
            Check(!ReserveFrameworkPress(dak,true,true,true,c),"Unsafe/nonservice input was captured");}
        Check(!ReserveFrameworkPress({Mode::hotkey,278},true,true,true,seated),"Dedicated mode changed DAK behavior");
        Check(!ReserveFrameworkPress({Mode::skyprompt,0},true,true,true,seated),"SkyPrompt mode changed DAK behavior");
    }
    {
        using namespace inn::service;
        for(const auto& row:scenes::ambient){
            Check(scenes::Allowed(row.id,row.parent),"Reviewed vanilla inn scene was treated as protected story control");
            if(row.parent)Check(!scenes::Allowed(row.id,row.parent+1),"Changed parent quest bypassed story protection");
            Check(row.name&&*row.name,"Reviewed scene lost its readable audit name");
        }
        Check(scenes::Allowed(0x6771C,0x6771B),"Logged Ivarstead inn scene 01 still blocked owner fallback");
        Check(scenes::Allowed(0x67720,0x6771F),"Logged Ivarstead inn scene 03 still blocked calling");
        Check(scenes::Allowed(0x67726,0x67725),"Ivarstead later banter scenes were omitted");
        Check(!scenes::Allowed(0x6771B,0x6771B),"Parent quest FormID was confused with a scene");
        Check(!scenes::Allowed(0,0)&&!scenes::Allowed(0xDEADB,0),"Unreviewed scene became interruptible");
        Check(control::ChooseRecovery(0,true,true)==control::Recovery::queuedRecheck,"First owned-Travel stall still destructively reinstalled AI");
        Check(control::ChooseRecovery(1,true,true)==control::Recovery::reinstall,"Repeated stalls could queue unlimited rechecks");
        Check(control::ChooseRecovery(2,true,true)==control::Recovery::handoff,"Stalled Travel bypassed the existing recovery limit");
        Check(control::ChooseRecovery(0,false,true)==control::Recovery::reinstall,"Non-owned control requested an engine recheck");
        Check(control::ChooseRecovery(0,true,false)==control::Recovery::reinstall,"Scene interruption was treated as a stationary path");
    }
    {
        using namespace inn::service::control;
        Check(inn::service::scenes::AllowedAliasPackage(0xDEE97,0xDEE92),"Vanilla server sandbox alias was blocked");
        Check(inn::service::scenes::AllowedAliasPackage(0xDEE98,0xDEE92),"Vanilla server greeting alias was blocked");
        Check(!inn::service::scenes::AllowedAliasPackage(0xDEE97,0x12345),"Reparented alias package bypassed foreign quest protection");
        Check(!inn::service::scenes::AllowedAliasPackage(0x12345,0xDEE92),"Unknown package in WITavern was blindly exempted");
        Check(Classify({})==Slot::empty,"Empty run-once slot was treated as occupied");
        Check(Classify({true,true,false,true,true,0xFE5CA82F,18,46})==Slot::owned,"Private Travel was not recognized as owned");
        for(auto type:{15u,28u}){
            Package conversation{true,false,true,false,false,0,type};
            Check(Classify(conversation)==Slot::engineDialogue,"Engine NPC dialogue was misclassified as a foreign mod lease");
            Check(Decide(Classify(conversation),false,false)==Action::interruptDialogue,"NPC dialogue could not yield to service");
            Check(Decide(Classify(conversation),true,false)==Action::protectedControl,"Actual player dialogue was interrupted");
            Check(Decide(Classify(conversation),false,true)==Action::protectedControl,"Protected quest scene was interrupted");
            conversation.hasQuest=true;
            Check(Classify(conversation)==Slot::external,"Quest-owned generated dialogue lost its protection");
        }
        for(auto type:{26u,31u,36u}){
            Package transition{true,false,true,false,false,0,type};
            Check(Classify(transition)==Slot::engineTransition,"Getting-up/movement transition cancelled service");
            Check(Decide(Classify(transition),false,false)==Action::waitTransition,"Engine movement transition was forcibly cleared");
        }
        Check(Classify({true,false,false,false,false,0xFF000123,15})==Slot::engineDialogue,"Runtime-ID dialogue was treated as an authored package");
        Check(Classify({true,false,false,true,false,0xFF000123,28})==Slot::engineDialogue,"Runtime clone source metadata hid an engine dialogue");
        Check(Classify({true,false,true,true,false,0x01001234,15})==Slot::external,"Authored mod dialogue was mistaken for engine chatter");
        Check(Classify({true,false,true,false,false,0xFF000123,6})==Slot::external,"Unknown generated Travel was overwritten");
        for(auto procedure:{10u,13u,33u})
            Check(Classify({true,false,true,false,false,0,18,procedure})==Slot::engineDialogue,"Generic runtime dialogue procedure lost service priority");
        for(auto procedure:{19u,34u,44u})
            Check(Classify({true,false,true,false,false,0,18,procedure})==Slot::engineTransition,"Generic runtime movement procedure was not given transition time");
        Check(Classify({true,false,true,false,false,0,18,46})==Slot::external,"Unknown runtime procedure was overwritten");
        Check(Classify({true,false,false,true,true,0xDEE91,18,46,true})==Slot::innRoutine,"Known vanilla inn action was treated as a foreign quest");
        Check(Decide(Slot::innRoutine,false,false)==Action::interruptDialogue,"Stopped vanilla routine could not hand off to service");
        Check(Decide(Slot::innRoutine,false,true)==Action::protectedControl,"Inn routine provenance overrode a protected active scene");
        Check(Decide(Slot::innRoutine,true,false)==Action::protectedControl,"Inn routine provenance overrode player dialogue");
        Check(Decide(Slot::empty,false,false)==Action::install,"Free control could not install Travel immediately");
        Check(Decide(Slot::owned,false,false)==Action::keep,"Owned Travel was unnecessarily reset");
        Check(Decide(Slot::external,false,false)==Action::protectedControl,"Foreign run-once package was stolen");
        Check(!WaitExpired(Action::waitTransition,transitionGrace-.1f),"Short getting-up transition ended the call");
        Check(WaitExpired(Action::waitTransition,transitionGrace),"Transition wait had no bounded deadline");
        Check(!WaitExpired(Action::protectedControl,protectedGrace-.1f),"No-alternative protected control ended immediately");
        Check(WaitExpired(Action::protectedControl,protectedGrace),"Protected control could hold service indefinitely");
        // An ordinary conversation can supersede control, then return to the
        // same private Travel slot without starting a new invitation.
        const auto interrupted=Decide(Slot::engineDialogue,false,false);
        const auto resumed=Decide(Slot::owned,false,false);
        Check(interrupted==Action::interruptDialogue&&resumed==Action::keep,"Conversation-resume sequence restarted or lost the service lease");
    }
    {
        using namespace inn::service::selection;
        std::vector<Candidate> staff{{10,Role::owner,1.f,true},{20,Role::waiter,900.f,true},{21,Role::waiter,400.f,true}};
        Check(Choose(staff)->id==21,"Closer owner displaced an available waiter");
        staff[2].available=false;
        Check(Choose(staff)->id==20,"Unavailable nearest waiter blocked the next waiter");
        staff[1].available=false;
        Check(Choose(staff)->id==10,"Unavailable waiters did not fall back to owner");
        staff.push_back({11,Role::owner,.5f,true});
        Check(Choose(staff)->id==11,"Owner fallback did not choose nearer owner");
        staff[0].distanceSquared=.5f;
        Check(Choose(staff)->id==10,"Equal-distance owners were not selected deterministically");
        std::vector<std::uint32_t> attempted{10};
        Check(Choose(staff,attempted)->id==11,"Failed owner was selected again in the same call");
        attempted.push_back(11);
        Check(!Choose(staff,attempted),"Exhausted candidates caused a retry loop");
        staff[1].available=true;staff[2].available=true;attempted={21};
        Check(Choose(staff,attempted)->id==20,"Failed waiter was not skipped before owner fallback");
        attempted.push_back(20);
        Check(Choose(staff,attempted)->role==Role::owner,"All attempted waiters did not advance to owner");
        staff={{0,Role::waiter,1.f,true},{50,Role::none,1.f,true},{51,Role::waiter,-1.f,true},
               {52,Role::waiter,std::numeric_limits<float>::infinity(),true},{53,Role::owner,9.f,true}};
        Check(Choose(staff)->id==53,"Invalid candidates blocked valid owner fallback");
        staff.back().available=false;
        Check(!Choose(staff),"No available staff still started a call");
        Check(!Choose({}),"Empty inn produced a service candidate");
        for(auto phase:{inn::service::Phase::preparing,inn::service::Phase::approaching}){
            Check(CanFallback(true,phase,1,12.f),"Before-dialogue approach could not change employee");
            Check(!CanFallback(false,phase,1,12.f),"Passive interaction changed its speaker");
        }
        for(auto phase:{inn::service::Phase::talking,inn::service::Phase::ordering,inn::service::Phase::releasing})
            Check(!CanFallback(true,phase,1,12.f),"Established dialogue/order changed employee automatically");
        Check(!CanFallback(true,inn::service::Phase::approaching,attemptLimit,12.f),"Fallback exceeded candidate limit");
        Check(!CanFallback(true,inn::service::Phase::approaching,2,callTimeout),"Fallback exceeded total call deadline");
        Check(CanFallback(true,inn::service::Phase::approaching,attemptLimit-1,callTimeout-.1f),"Last bounded fallback was rejected");
        Check(!CanFallback(true,inn::service::Phase::approaching,1,-1.f),"Invalid fallback elapsed time accepted");
        Check(!CanFallback(true,inn::service::Phase::approaching,1,std::numeric_limits<float>::quiet_NaN()),"Non-finite fallback elapsed time accepted");
    }
    for(auto phase:{inn::service::Phase::preparing,inn::service::Phase::approaching}) {
        Check(inn::service::RepeatBellAllowed(true,phase),"A second bell press could not ring during the existing approach");
        Check(!inn::service::RepeatBellAllowed(false,phase),"Passive staff service was treated as a repeated bell call");
    }
    for(auto phase:{inn::service::Phase::talking,inn::service::Phase::ordering,inn::service::Phase::releasing})
        Check(!inn::service::RepeatBellAllowed(true,phase),"A later order phase allowed a new approach request");
    using namespace inn::call;
    Context valid{true,true,false,false,false,true};
    Settings framework{Mode::framework,48},hotkey{Mode::hotkey,48};
    Check(Trigger(framework,18,true,true,true,true,valid),"DAK+Activate not accepted");
    Check(!Trigger(framework,48,true,false,true,true,valid),"Framework mode still used independent key");
    Check(!Trigger(framework,18,true,true,false,true,valid),"Missing framework fell back to a key");
    Check(!Trigger(framework,18,true,true,true,false,valid),"Plain Activate called service");
    Check(!Trigger(framework,18,false,true,true,true,valid),"Held key repeatedly called service");
    Check(Trigger(hotkey,48,true,false,false,false,valid),"Dedicated key depends on DAK");
    Check(!Trigger(hotkey,18,true,true,true,true,valid),"Hotkey mode still used DAK activation");
    Check(!Trigger({Mode::hotkey,0},0,true,false,false,false,valid),"Unbound mode reserved a key");
    const Settings sky{Mode::skyprompt,48};
    Check(PromptTrigger(sky,true,valid),"SkyPrompt call unavailable in seated context");
    Check(!PromptTrigger(sky,false,valid),"Missing SkyPrompt accepted call");
    Check(!PromptTrigger(hotkey,true,valid),"Inactive SkyPrompt mode triggered");
    Check(!Trigger(sky,48,true,true,true,true,valid),"SkyPrompt mode stole an independent/DAK key");
    auto blocked=valid;blocked.safeTarget=false;
    Check(!Trigger(framework,18,true,true,true,true,blocked),"Food or another active target lost its Activate action");
    Check(Trigger(hotkey,48,true,false,false,false,blocked),"Dedicated key incorrectly depends on empty crosshair");
    Check(PromptTrigger(sky,true,blocked),"Third-person SkyPrompt call incorrectly requires an empty crosshair");
    for(int i=0;i<5;++i){auto c=valid;switch(i){case 0:c.seated=false;break;case 1:c.inInn=false;break;case 2:c.inCombat=true;break;case 3:c.menuOpen=true;break;case 4:c.busy=true;break;}
        Check(!Trigger(framework,18,true,true,true,true,c),"Unsafe framework context accepted");
        Check(!Trigger(hotkey,48,true,false,false,false,c),"Unsafe hotkey context accepted");}
    Check(Normalize(99,-7)==Settings{},"Invalid stored settings did not fail safely");
    Check(Normalize(0,282)==Settings{Mode::hotkey,0},"Out-of-range key accepted");
    Check(Normalize(1,268)==Settings{Mode::framework,268},"Switch lost saved gamepad binding");
    Check(!Valid({static_cast<Mode>(3),42}),"Invalid mode accepted");
    Check(!Valid({Mode::hotkey,999}),"Invalid hotkey accepted");
    const std::string original="[General]\nLanguage=zh\n[Menu]\nScalePercent=85\n[Call]\nMode=0\nHotkey=48\n; user note\nExtraSetting=7\n[Other]\nValue=kept\n";
    auto updated=UpdatedIni(original,{Mode::framework,268});
    Check(updated.find("Language=zh")!=std::string::npos,"Saving entry changed language");
    Check(updated.find("ScalePercent=85")!=std::string::npos,"Saving entry changed layout");
    Check(updated.find("; user note")!=std::string::npos&&updated.find("ExtraSetting=7")!=std::string::npos,"Unknown settings lost");
    Check(updated.find("[Other]\r\nValue=kept")!=std::string::npos,"Unrelated section lost");
    Check(updated.find("Mode=1\r\nHotkey=268\r\nShowHotkeyHint=0\r\nPromptUsesHotkey=0")!=std::string::npos,"Settings written to wrong section");
    Check(UpdatedIni(updated,{Mode::framework,268})==updated,"Repeated save duplicated settings");
    Check(UpdatedIni("[General]\nLanguage=auto\n",{}).find("[Call]\r\nMode=1\r\nHotkey=0")!=std::string::npos,"Missing call section not added");
    auto casing=UpdatedIni("[CALL]\n  mode = 0\nHOTKEY=42\n",{});
    Check(casing.find("mode =")==std::string::npos&&casing.find("HOTKEY=")==std::string::npos,"Case variants retained duplicate keys");
    using inn::service::Progress;using inn::service::Step;using inn::service::Decide;
    Progress walking{true,true,false,false,false,false,1};
    Check(Decide(walking)==Step::wait,"Call opened conversation before staff arrived");
    auto service=walking;service.atTable=true;Check(Decide(service)==Step::arrive,"Arrived staff did not start conversation");
    service=walking;service.context=false;Check(Decide(service)==Step::cancel,"Leaving seat/cell did not cancel approach");
    service=walking;service.server=false;Check(Decide(service)==Step::cancel,"Unavailable/dead staff kept approaching");
    service=walking;service.elapsed=35;Check(Decide(service)==Step::failed,"Blocked path had no bounded timeout");
    service.paused=true;Check(Decide(service)==Step::wait,"Paused menu incorrectly failed service");
    service=walking;service.pathDone=true;Check(Decide(service)==Step::failed,"Failed native path accepted");
    service.pathSuccess=true;Check(Decide(service)==Step::failed,"Path stopped too far away accepted as arrival");
    service.atTable=true;Check(Decide(service)==Step::arrive,"Reached distance discarded due completed path");
    Check(Normalize(2,268)==Settings{Mode::skyprompt,268},"Stored SkyPrompt mode lost or erased hotkey");
    auto own=Settings{Mode::hotkey,48};own.showHotkeyHint=true;
    Check(Trigger(own,48,true,false,false,false,valid),"Dedicated hotkey required holding instead of immediate press");
    Check(!PromptTrigger(own,true,valid),"Display-only hint incorrectly dispatched a call");
    Check(!Trigger(own,48,false,false,false,false,valid),"Held dedicated key dispatched repeated calls");
    own.promptUsesHotkey=true;
    auto custom=UpdatedIni(original,own);
    Check(custom.find("ShowHotkeyHint=1\r\nPromptUsesHotkey=1")!=std::string::npos,"Prompt settings were not persisted");
    Check(custom.find("Language=zh")!=std::string::npos&&custom.find("ScalePercent=85")!=std::string::npos,"Prompt settings replaced unrelated config");
    Check(UpdatedIni(custom,own)==custom,"Repeated prompt-setting save duplicated keys");
    auto chat=walking;chat.chatting=true;chat.pathDone=true;
    Check(Decide(chat)==Step::wait,"NPC chat interruption cancelled service instead of suspending");
    chat.elapsed=35;Check(Decide(chat)==Step::failed,"Long NPC scene locked service forever");
    chat.paused=true;Check(Decide(chat)==Step::wait,"Paused scene incorrectly exhausted service timeout");
    chat.context=false;Check(Decide(chat)==Step::cancel,"Leaving seat during chat did not cancel");
    auto retry=walking;retry.pathDone=true;retry.retries=0;retry.outcomeAge=.4f;
    Check(Decide(retry)==Step::wait,"Path failure got no scheduling grace");
    retry.outcomeAge=.6f;Check(Decide(retry)==Step::retry,"Transient failed path not retried");
    retry.retries=1;Check(Decide(retry)==Step::failed,"Failed path retried without a limit");
    retry.elapsed=35;retry.retries=0;Check(Decide(retry)==Step::failed,"Retry extended total timeout indefinitely");
    Check(Settings{}.prioritizeService,"Priority service was not enabled by default");
    Check(!Settings{}.autoClearBeforeTalk,"Automatic cleanup unexpectedly enabled by default");
    Check(!Settings{}.clearAllTableItems,"All-items cleanup unexpectedly enabled by default");
    auto all=own;all.clearAllTableItems=true;
    const auto allIni=UpdatedIni(original,all);
    Check(allIni.find("ClearAllTableItems=1")!=std::string::npos,"All-items cleanup option not persisted");
    Check(allIni.find("Language=zh")!=std::string::npos&&allIni.find("ScalePercent=85")!=std::string::npos,"Cleanup option replaced unrelated preferences");
    Check(UpdatedIni(allIni,all)==allIni,"Cleanup setting duplicated on repeated save");
    all.clearAllTableItems=false;
    const auto normalIni=UpdatedIni(allIni,all);
    Check(normalIni.find("ClearAllTableItems=0")!=std::string::npos&&normalIni.find("ClearAllTableItems=1")==std::string::npos,"Switching back to normal cleanup did not persist");
    auto serviceOptions=own;serviceOptions.prioritizeService=false;serviceOptions.autoClearBeforeTalk=true;
    auto persisted=UpdatedIni(original,serviceOptions);
    Check(persisted.find("PrioritizeService=0\r\nAutoClearBeforeTalk=1")!=std::string::npos,"Service options not saved");
    Check(UpdatedIni(persisted,serviceOptions)==persisted,"Service option save duplicated config");
    Check(!Settings{}.hideClearTableDialogue&&!Settings{}.hideRestoreTableDialogue,"Cleanup dialogue options were hidden by default");
    for(bool hideClear:{false,true})for(bool hideRestore:{false,true}){
        auto visibility=serviceOptions;
        visibility.hideClearTableDialogue=hideClear;
        visibility.hideRestoreTableDialogue=hideRestore;
        const auto text=UpdatedIni(original,visibility);
        Check(text.find(std::string("HideClearTableDialogue=")+(hideClear?"1":"0"))!=std::string::npos,"Clear dialogue visibility was not saved independently");
        Check(text.find(std::string("HideRestoreTableDialogue=")+(hideRestore?"1":"0"))!=std::string::npos,"Restore dialogue visibility was not saved independently");
        Check(text.find("AutoClearBeforeTalk=1")!=std::string::npos&&text.find("Hotkey=48")!=std::string::npos,"Hiding dialogue changed automatic cleanup or the call binding");
        Check(text.find("Language=zh")!=std::string::npos&&text.find("ExtraSetting=7")!=std::string::npos,"Hiding dialogue replaced unrelated preferences");
        Check(UpdatedIni(text,visibility)==text,"Repeated visibility save duplicated INI keys");
        auto onlyClear=visibility;onlyClear.hideClearTableDialogue=!hideClear;
        const auto changed=UpdatedIni(text,onlyClear);
        Check(changed.find(std::string("HideRestoreTableDialogue=")+(hideRestore?"1":"0"))!=std::string::npos,"Toggling clear dialogue changed restore dialogue visibility");
        auto onlyRestore=visibility;onlyRestore.hideRestoreTableDialogue=!hideRestore;
        const auto restored=UpdatedIni(text,onlyRestore);
        Check(restored.find(std::string("HideClearTableDialogue=")+(hideClear?"1":"0"))!=std::string::npos,"Toggling restore dialogue changed clear dialogue visibility");
    }
    const auto mixedVisibility=UpdatedIni("[CALL]\n hidecleartabledialogue = 1\n HIDERestORETABLEDialogue=1\nUnknown=retained\n",{});
    Check(mixedVisibility.find("hidecleartabledialogue =")==std::string::npos&&mixedVisibility.find("HIDERestORETABLEDialogue=")==std::string::npos,"Visibility INI keys retained duplicates with different casing");
    Check(mixedVisibility.find("HideClearTableDialogue=0\r\nHideRestoreTableDialogue=0")!=std::string::npos&&mixedVisibility.find("Unknown=retained")!=std::string::npos,"Disabling both options did not restore visibility or preserve unknown keys");
    using inn::service::Motion;using inn::service::NeedsRecovery;using inn::service::MatchesOrder;
    Motion motion;motion.Reset(100,200,30);
    motion.Observe(100,200,30,3.9f);Check(!NeedsRecovery(motion.still,0),"Brief idle immediately reset AI");
    motion.Observe(100,200,30,.1f);Check(NeedsRecovery(motion.still,0),"Four-second stall never recovered");
    motion.Observe(115,200,30,.1f);Check(motion.still==0,"Walking still accumulated stall time");
    motion.Reset(0,0,0);motion.Observe(5,0,0,1);motion.Observe(10,0,0,1);motion.Observe(15,0,0,1);
    Check(motion.still==0,"Small accumulated walking steps looked stuck");
    motion.Observe(15,0,15,.1f);Check(motion.still==0,"Vertical navigation progress looked stuck");
    Check(!NeedsRecovery(0,.74f),"Transient completed package caused immediate repeated inserts");
    Check(NeedsRecovery(0,.75f),"Lost package never recovered");
    Check(inn::service::recoveryLimit==2,"Path recovery is unbounded");
    Check(inn::service::resetSettle>0,"Reset and package installation use the same update");
    Check(MatchesOrder(12,12),"Matching completed order did not release service");
    Check(!MatchesOrder(12,11),"Older cancellation released a newer order");
    Check(!MatchesOrder(0,0),"Empty token released an active service");
    Check(!MatchesOrder(0,12),"Untracked order released active service");
    using inn::service::AutoCleanupAllowed;using inn::service::CleanupEntry;
    using inn::service::CleanupVisit;using inn::service::Phase;
    CleanupVisit visit;
    auto allowed=[&](CleanupEntry entry,Phase phase,bool called=false,bool enabled=true,
                     bool cleaned=false,bool matchingContext=true){
        return AutoCleanupAllowed(entry,phase,called,visit,enabled,cleaned,matchingContext);
    };
    Check(!allowed(CleanupEntry::orderBegin,Phase::talking),"Unseated player acquired automatic cleanup permission");
    Check(visit.Observe(101,201,301),"First seated visit was not detected");
    // Test every state, including a fresh call and a sealed paid-meal visit.
    // No combination may let a conversation or arrival trigger cleanup.
    for(bool sealed:{false,true})for(bool called:{false,true}){
        visit.sealed=sealed;
        for(Phase phase:{Phase::preparing,Phase::approaching,Phase::talking,Phase::ordering,Phase::releasing}){
            for(CleanupEntry entry:{CleanupEntry::conversation,CleanupEntry::arrival}){
                Check(!allowed(entry,phase,called),"Conversation or arrival cleared the table without ordering");
            }
            const bool before=phase==Phase::preparing||phase==Phase::approaching||phase==Phase::talking;
            const bool expected=before&&(!sealed||called);
            Check(allowed(CleanupEntry::orderBegin,phase,called)==expected,"Ordering cleanup permission disagreed with service state");
            Check(!allowed(CleanupEntry::orderBegin,phase,called,false),"Disabled automatic cleanup still ran");
            Check(!allowed(CleanupEntry::orderBegin,phase,called,true,true),"Successful cleanup ran a second time");
            Check(!allowed(CleanupEntry::orderBegin,phase,called,true,false,false),"Changed seat/cell kept an old cleanup target");
        }
    }
    visit.sealed=false;
    Check(!visit.Observe(101,201,301)&&visit.Pending(),"Unchanged seat discarded pending ordering cleanup");
    Check(!allowed(CleanupEntry::conversation,Phase::talking),"Passive innkeeper conversation cleared the table");
    Check(visit.Pending(),"Conversation used up cleanup before an actual order");
    Check(!allowed(CleanupEntry::arrival,Phase::approaching,true),"Ringing and waiter arrival cleared the table");
    Check(allowed(CleanupEntry::orderBegin,Phase::talking),"Direct ordering required ringing first");
    visit.Seal();
    Check(!allowed(CleanupEntry::orderBegin,Phase::talking),"Reopening the ordering menu repeated same-visit cleanup");
    Check(!allowed(CleanupEntry::conversation,Phase::talking,true),"Ringing rearmed cleanup in plain conversation");
    Check(allowed(CleanupEntry::orderBegin,Phase::talking,true),"A new called order could not clear leftovers");
    Check(visit.sealed,"Checking a called order rearmed later passive conversations");
    CleanupVisit failed;
    failed.Observe(101,201,301);
    Check(AutoCleanupAllowed(CleanupEntry::orderBegin,Phase::talking,false,failed,true,false,true),"Initial ordering cleanup was skipped");
    failed.Seal(); // Seal even on failure; new food must never face a late retry.
    Check(!AutoCleanupAllowed(CleanupEntry::orderBegin,Phase::talking,false,failed,true,false,true),"Failed cleanup retried after menu reopening");
    Check(!AutoCleanupAllowed(CleanupEntry::orderBegin,Phase::ordering,true,failed,true,false,true),"Failed cleanup retried after food placement started");
    Check(visit.Observe(0,201,301)&&!visit.Pending(),"Standing up retained ordering cleanup permission");
    Check(visit.Observe(101,201,301)&&visit.Pending(),"Sitting down again did not start a new visit");
    visit.Seal();
    Check(visit.Observe(102,201,301)&&visit.Pending(),"Changing seats did not start a new visit");
    Check(!allowed(CleanupEntry::conversation,Phase::talking),"Changing seats allowed cleanup before ordering");
    visit.Seal();
    Check(visit.Observe(102,202,301)&&visit.Pending(),"Changing cells did not start a new visit");
    visit.Seal();
    Check(visit.Observe(102,202,302)&&visit.Pending(),"Changing inns did not start a new visit");
    Check(visit.Observe(102,0,302)&&!visit.Pending(),"Missing cell allowed passive cleanup");
    Check(visit.Observe(102,202,0)&&!visit.Pending(),"Missing inn allowed passive cleanup");
    CleanupVisit loadedVisit;
    loadedVisit.Observe(101,201,301);
    loadedVisit.Seal(); // The runtime paid-table guard seals restored meals.
    Check(!AutoCleanupAllowed(CleanupEntry::orderBegin,Phase::talking,false,loadedVisit,true,false,true),"Restored paid meal lost automatic cleanup protection");
    loadedVisit.Observe(101,201,301);
    Check(!AutoCleanupAllowed(CleanupEntry::orderBegin,Phase::talking,false,loadedVisit,true,false,true),"Paid-meal protection vanished on the next seated update");
    Check(!AutoCleanupAllowed(CleanupEntry::conversation,Phase::talking,true,loadedVisit,true,false,true),"New bell cleared a restored paid meal during conversation");
    Check(AutoCleanupAllowed(CleanupEntry::orderBegin,Phase::talking,true,loadedVisit,true,false,true),"Restored-meal guard blocked cleanup for a new called order");
    std::cout<<checks<<" call entry and settings checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
