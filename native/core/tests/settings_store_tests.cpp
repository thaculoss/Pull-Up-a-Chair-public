#include "SettingsStore.h"
#include "CallPolicy.h"
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <atomic>
using namespace inn::settingsstore;
int checks{};
void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
int main(){
    const auto folder=std::filesystem::temp_directory_path()/L"inn-ordering-settings-tests"/std::to_wstring(GetCurrentProcessId());
    const auto file=folder/L"PullUpAChairOrdering.ini";
    try {
        std::filesystem::create_directories(folder);
        const std::string seed="\xef\xbb\xbf; 保留用户注释\n[General]\nLanguage=zh\n[Serving]\nTargetMainDiameter=14\nCompactMainDishes=1\n\n[Unrelated]\nKeep=用户自定义\n";
        {std::ofstream out(file,std::ios::binary);out<<seed;}
        auto save=[&](std::string_view section,const Entries& entries){return Save([&](auto before){return Update(before,section,entries);},file.c_str());};
        std::string disk;Check(Read(disk,file.c_str())&&disk==seed,"Initial file read failed");
        Check(save("Serving",{{"TargetMainDiameter","22"},{"CompactMainDishes","1"},{"CompactDesserts","0"},{"TargetDessertDiameter","18"}}),"Serving write failed");
        Check(Read(disk,file.c_str())&&Get(disk,"Serving","TargetMainDiameter","")=="22","Serving diameter failed fresh disk read");
        inn::call::Settings call;call.autoClearBeforeTalk=true;call.hideRestoreTableDialogue=true;
        Check(Save([&](auto before){return inn::call::UpdatedIni(before,call);},file.c_str()),"Call write failed");
        Check(save("General",{{"Language","en"}}),"Language write failed");
        Check(Read(disk,file.c_str()),"Fresh settings reload failed");
        Check(Get(disk,"Serving","TargetMainDiameter","")=="22"&&Get(disk,"Serving","TargetDessertDiameter","")=="18","Call/language writer reverted serving size");
        Check(Get(disk,"General","Language","")=="en"&&Integer(disk,"Call","HideRestoreTableDialogue",0)==1,"Latest settings lost");
        Check(disk.starts_with("\xef\xbb\xbf")&&disk.find("保留用户注释")!=disk.npos&&Get(disk,"Unrelated","Keep","")=="用户自定义","BOM/comments/unrelated section damaged");
        // Simulate concurrent settings pages repeatedly updating distinct sections.
        std::atomic_bool okay{true};
        std::thread a([&]{for(int i=0;i<25;++i)if(!save("Serving",{{"TargetMainDiameter",std::to_string(i)}}))okay=false;});
        std::thread b([&]{for(int i=0;i<25;++i)if(!save("General",{{"Language",i==24?"zh":"en"}}))okay=false;});
        a.join();b.join();Check(okay,"Concurrent settings update failed");
        Check(Read(disk,file.c_str())&&Get(disk,"Serving","TargetMainDiameter","")=="24"&&Get(disk,"General","Language","")=="zh","Concurrent update lost another section");
        const auto before=disk;
        HANDLE locked=CreateFileW(file.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
        Check(locked!=INVALID_HANDLE_VALUE,"Test lock failed");
        Check(!save("Serving",{{"TargetMainDiameter","22"}}),"Save succeeded despite denied write access");CloseHandle(locked);
        Check(Read(disk,file.c_str())&&disk==before,"Failed save altered existing file");
        Check(!Save([](auto){return std::string("new");},(folder/L"missing.ini").c_str()),"Missing provider silently created another config");
        Check(!Save([](auto)->std::string{throw std::runtime_error("failure");},file.c_str()),"Failed update reported success");
        Check(!Save([](auto){return std::string(1024*1024+1,'x');},file.c_str()),"Oversized output accepted");
        Check(Read(disk,file.c_str())&&disk==before,"Prewrite validation changed file");
        auto duplicate=Update("[Serving]\nTargetMainDiameter=14\n[Serving]\nTargetMainDiameter=18\n[Other]\nK=V\n","Serving",{{"TargetMainDiameter","22"}});
        Check(Get(duplicate,"Serving","TargetMainDiameter","")=="22"&&duplicate.find("=14")==duplicate.npos&&duplicate.find("=18")==duplicate.npos,"Duplicate section retained stale value");
        Check(Get(Update("[GENERAL]\nLANGUAGE=en","General",{{"Language","zh"}}),"general","language","")=="zh","Case-insensitive key update failed");
        Check(Integer("[S]\nI=broken","S","I",14)==14,"Malformed integer lost fallback");
        const auto bomCall=inn::call::UpdatedIni("\xef\xbb\xbf[Call]\nMode=0\nHotkey=44\n[Serving]\nTargetMainDiameter=22",call);
        Check(Integer(bomCall,"Call","Mode",-1)==1&&Integer(bomCall,"Call","Hotkey",-1)==0,"BOM-prefixed Call section kept stale settings");
        std::filesystem::remove(file);std::filesystem::remove(folder);
        std::cout<<checks<<" settings persistence checks passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
