#include "ServicePolicy.h"
#include "MigrationPolicy.h"
#include "RuntimePolicy.h"
#include "BindingPolicy.h"
#include "BindingStore.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
int checks=0;
void Check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
int main() {
    using namespace innservice;
    try {
        Check(!CanMigrate(false,true,false),"DataLoaded started migration before a loaded game");
        Check(!CanMigrate(true,false,false),"Migration started without a loaded player cell");
        Check(!CanMigrate(true,true,true),"Migration started in a blocking menu");
        Check(CanMigrate(true,true,false),"Playable loaded game did not start migration");
        MigrationProgress migration;
        for(int i=0;i<600;++i)Check(!migration.Advance(1.f,false,false),"Main menu consumed migration deadline");
        for(int i=0;i<600;++i)Check(!migration.Advance(1.f,true,true),"Paused/loading game consumed migration deadline");
        Check(migration.activeSeconds==0.f,"Blocked time was accumulated");
        for(int i=0;i<19;++i)Check(!migration.Advance(1.f,true,false),"Migration was declared slow early");
        Check(migration.Advance(1.f,true,false),"Slow migration lacked diagnostics");
        for(int i=0;i<120;++i)Check(!migration.Advance(1.f,true,false),"Slow migration repeated warning every tick");
        Check(migration.activeSeconds==140.f,"Slow migration stopped progressing after 20 seconds");
        migration={};Check(!migration.Advance(1000.f,true,false)&&migration.activeSeconds==1.f,"A stalled frame exhausted migration budget");
        migration={};Check(!migration.Advance(-1.f,true,false)&&migration.activeSeconds==0.f,"Negative delta accepted");
        Check(!migration.Advance(std::numeric_limits<float>::quiet_NaN(),true,false),"NaN poisoned migration progress");
        std::vector<Staff> staff{{1,1,10,20,30,40,Role::owner,100,true},{2,2,10,20,30,40,Role::waiter,300,true}};
        Check(ChooseServer(staff,10,20,1800)==2,"Dedicated waiter not preferred over owner");
        staff[1].available=false;Check(ChooseServer(staff,10,20,1800)==1,"Owner fallback failed");
        staff[0].available=false;Check(ChooseServer(staff,10,20,1800)==0,"Unavailable staff called");
        staff[0].available=true;staff[0].inn=11;Check(ChooseServer(staff,10,20,1800)==0,"Different inn staff called");
        staff[0].inn=10;staff[0].cell=21;Check(ChooseServer(staff,10,20,1800)==0,"Different room actor called through unloaded path");
        staff[0].cell=20;staff[0].distance=1900;Check(ChooseServer(staff,10,20,1800)==0,"Out of range staff called");
        staff[0].distance=std::numeric_limits<float>::quiet_NaN();Check(ChooseServer(staff,10,20,1800)==0,"NaN distance accepted");
        staff={{1,1,10,20,30,40,Role::waiter,200,true},{2,2,10,20,30,40,Role::waiter,100,true}};
        Check(ChooseServer(staff,10,20,1800)==2,"Nearest waiter not preferred");
        staff[0].distance=100;Check(ChooseServer(staff,10,20,1800)==1,"Equal distance not deterministic");
        staff[0].role=Role::none;Check(ChooseServer(staff,10,20,1800)==2,"Guest classified as employee");
        std::vector<Vendor> vendors{{10,30,40,1,true,true},{10,30,40,2,true,true},{10,31,41,0,true,false},{11,32,42,3,true,true}};
        auto supply=ChooseSupply(10,vendors,2);
        Check(supply.chest==40&&supply.merchant==1&&supply.inn==10,"Innkeeper stock not canonical");
        std::reverse(vendors.begin(),vendors.end());Check(ChooseSupply(10,vendors,2).chest==supply.chest,"Stock depends on actor scan order");
        Check(ChooseSupply(10,{{10,30,40,0,true,false}},0).status==2,"Owner outside room lost persistent stock");
        Check(ChooseSupply(10,{{10,30,40,0,true,false},{10,31,41,0,true,false}},0).status==3,"Ambiguous chest silently selected");
        Check(ChooseSupply(10,{{10,30,40,0,true,false},{10,31,40,0,true,false}},0).chest==40,"Shared chest aliases treated as different stock");
        Check(!ChooseSupply(10,{{11,30,40,1,true,true}},0).chest,"Other inn inventory stolen");
        Check(!ChooseSupply(10,{{10,30,40,1,false,false}},0).chest,"Unrelated vendor stock selected");
        Check(!ChooseSupply(10,{},0).chest,"Missing stock fabricated");
        Context c{true,true,true,true,false,true};Check(Continues(c),"Valid request cancelled");
        c.seated=false;Check(!Continues(c),"Standing did not cancel");c.seated=true;
        c.sameSeat=false;Check(!Continues(c),"Changing chairs did not cancel");c.sameSeat=true;
        c.sameCell=false;Check(!Continues(c),"Changing rooms did not cancel");c.sameCell=true;
        c.sameInn=false;Check(!Continues(c),"Changing inns did not cancel");c.sameInn=true;
        c.combat=true;Check(!Continues(c),"Combat service continued");c.combat=false;
        c.staffValid=false;Check(!Continues(c),"Dead or departed actor retained");
        Check(Arrived(140,40,145),"Near actor not arrived");
        Check(!Arrived(140,100,145),"Actor on another floor considered arrived");
        Check(!Arrived(180,0,145),"Far actor considered arrived");
        Check(!Expired(20,30)&&Expired(30,30),"Approach timeout wrong");
        PadChord pad;Check(!pad.Update(0x100,true,0x300),"Single shoulder triggered call");
        Check(pad.Update(0x200,true,0x300),"Two shoulders did not trigger call");
        Check(!pad.Update(0x200,true,0x300),"Held chord repeated request");
        Check(!pad.Update(0x100,false,0x300)&&pad.Update(0x100,true,0x300),"Chord re-press failed");
        Check(!pad.Update(0x400,true,0),"Disabled chord triggered");
        Check(PadButton(9)==0x10000&&PadButton(10)==0x20000,"Triggers confused with directional masks");
        Check(PadButton(0x100)==0x100&&PadButton(0x400)==0,"Invalid raw pad button accepted");
        BindingRecorder recorder;recorder.Begin(CaptureDevice::keyboard);
        Check(recorder.Event(CaptureDevice::keyboard,37,false,false).action==CaptureAction::none,"Held opener counted as fresh key");
        Check(recorder.Event(CaptureDevice::gamepad,0x1000,true,false).action==CaptureAction::none,"Wrong device changed keyboard binding");
        Check(recorder.Event(CaptureDevice::keyboard,37,true,false).action==CaptureAction::none,"Keyboard binding completed before release");
        auto result=recorder.Event(CaptureDevice::keyboard,37,false,true);
        Check(result.action==CaptureAction::complete&&result.binding==37&&recorder.mode==CaptureDevice::none,"Keyboard release capture failed");
        recorder.Begin(CaptureDevice::gamepad);
        Check(recorder.Event(CaptureDevice::gamepad,0x1000,false,true).action==CaptureAction::none,"Opener release bound A");
        recorder.Event(CaptureDevice::gamepad,0x100,true,false);recorder.Event(CaptureDevice::gamepad,0x200,true,false);
        Check(recorder.Event(CaptureDevice::gamepad,0x100,false,true).action==CaptureAction::none,"Chord completed while RB held");
        result=recorder.Event(CaptureDevice::gamepad,0x200,false,true);
        Check(result.action==CaptureAction::complete&&result.binding==0x300,"Two-button chord capture failed");
        recorder.Begin(CaptureDevice::gamepad);recorder.Event(CaptureDevice::gamepad,0x2000,true,false);
        result=recorder.Event(CaptureDevice::gamepad,0x2000,false,true);
        Check(result.action==CaptureAction::complete&&result.binding==0x2000,"B cannot be assigned as a pad binding");
        recorder.Begin(CaptureDevice::gamepad);recorder.Event(CaptureDevice::gamepad,9,true,false);recorder.Event(CaptureDevice::gamepad,10,true,false);
        Check(recorder.Event(CaptureDevice::gamepad,9,false,true).action==CaptureAction::none,"LT+RT completed early");
        Check(recorder.Event(CaptureDevice::gamepad,10,false,true).binding==0x30000,"Trigger chord failed");
        recorder.Begin(CaptureDevice::gamepad);result=recorder.Event(CaptureDevice::keyboard,1,true,false);
        Check(result.action==CaptureAction::cancelled&&recorder.mode==CaptureDevice::none,"Escape did not cancel gamepad capture");
        Check(recorder.Event(CaptureDevice::gamepad,0x100,true,false).action==CaptureAction::none,"Cancelled capture accepted input");
        recorder.Begin(CaptureDevice::keyboard);Check(recorder.Event(CaptureDevice::keyboard,999,true,false).action==CaptureAction::none,"Invalid keyboard code captured");
        const auto folder=std::filesystem::temp_directory_path()/("inn-service-bindings-"+std::to_string(GetCurrentProcessId()));
        std::filesystem::create_directories(folder);
        const auto base=(folder/"defaults.ini").string(),user=(folder/"user.ini").string();
        {std::ofstream out(base);out<<"[Input]\nKeyboardKey=37\nGamepadChord=768\n";}
        {std::ofstream out(user);out<<"[Other]\nPreserve=123\n";}
        auto saved=ReadBindings(base.c_str(),user.c_str());Check(saved.keyboard==37&&saved.gamepad==768,"Default bindings failed");
        Check(WriteKeyboardBinding(user.c_str(),44)&&WriteGamepadBinding(user.c_str(),0x30000),"User bindings write failed");
        saved=ReadBindings(base.c_str(),user.c_str());Check(saved.keyboard==44&&saved.gamepad==0x30000,"User bindings not reloaded");
        Check(GetPrivateProfileIntA("Other","Preserve",0,user.c_str())==123,"Binding write lost unrelated setting");
        Check(WriteKeyboardBinding(user.c_str(),0)&&WriteGamepadBinding(user.c_str(),0),"Disable binding write failed");
        saved=ReadBindings(base.c_str(),user.c_str());Check(!saved.keyboard&&!saved.gamepad,"Disabled bindings did not persist");
        Check(!WriteKeyboardBinding(user.c_str(),1)&&!WriteGamepadBinding(user.c_str(),0x400),"Reserved input accepted");
        Check(WriteKeyboardBinding(user.c_str(),37)&&WriteGamepadBinding(user.c_str(),768),"Defaults restore failed");
        saved=ReadBindings(base.c_str(),user.c_str());Check(saved.keyboard==37&&saved.gamepad==768,"Restored bindings not persisted");
        std::filesystem::remove(base);std::filesystem::remove(user);std::filesystem::remove(folder);
        for(auto v:inn::supportedRuntimes){Check(inn::Supported(v),"Target version rejected");Check(!inn::ExperimentalVR(v),"Flat runtime marked experimental VR");}
        Check(inn::Supported({1,4,15,0})&&inn::ExperimentalVR({1,4,15,0}),"Exact experimental VR runtime rejected");
        for(auto v:{inn::RuntimeVersion{1,4,14,0},inn::RuntimeVersion{1,4,16,0},inn::RuntimeVersion{1,4,15,1}})
            Check(!inn::Supported(v)&&!inn::ExperimentalVR(v),"Unknown VR runtime accepted");
        Check(!inn::Supported({1,7,999,0}),"Unknown future accepted");
        for(auto phase:{api::Phase::starting,api::Phase::approaching,api::Phase::waiting})
            Check(RepeatBellAllowed(static_cast<std::uint32_t>(phase)),"A repeated bell press was blocked while waiting for staff");
        for(auto phase:{api::Phase::idle,api::Phase::conversation,api::Phase::releasing})
            Check(!RepeatBellAllowed(static_cast<std::uint32_t>(phase)),"Repeated bell incorrectly restarted another service phase");
        std::cout<<checks<<" service policy checks passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
