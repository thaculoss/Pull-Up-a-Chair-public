#pragma once
#include "PCH.h"
#include <Windows.h>
namespace inn::recorded {
// Call only on the game thread. The engine owns each one-shot after Play().
// New bell presses may overlap the natural decay of the previous bell.
inline bool Play(std::uint32_t local,const char* plugin,const wchar_t* ini,
                 const wchar_t* enabled,const wchar_t* volume,const char* cue) {
    if(!GetPrivateProfileIntW(L"Sounds",enabled,1,ini))return false;
    const auto percent=std::clamp(static_cast<int>(GetPrivateProfileIntW(L"Sounds",volume,100,ini)),0,100);
    if(!percent)return false;
    const auto data=RE::TESDataHandler::GetSingleton();const auto manager=RE::BSAudioManager::GetSingleton();
    const auto descriptor=data?data->LookupForm<RE::BGSSoundDescriptorForm>(local,plugin):nullptr;
    if(!manager||!descriptor){logger::warn("Recorded interaction sound unavailable cue={} plugin={} local={:X}",cue,plugin,local);return false;}
    RE::BSSoundHandle sound;
    if(!manager->GetSoundHandle(sound,descriptor))return false;
    if(const auto player=RE::PlayerCharacter::GetSingleton())sound.SetPosition(player->GetPosition());
    sound.SetVolume(percent/100.f);
    const bool started=sound.Play();
    logger::info("Recorded interaction sound cue={} started={} volume={}",cue,started,percent);
    return started;
}
inline void PageTurn(){Play(0xD00,"Pull up a chair Ordering.esp",L".\\Data\\SKSE\\Plugins\\PullUpAChairOrdering.ini",L"PageTurnEnabled",L"PageTurnVolumePercent","page_turn");}
inline void OrderingBell(){Play(0xD01,"Pull up a chair Ordering.esp",L".\\Data\\SKSE\\Plugins\\PullUpAChairOrdering.ini",L"ServiceBellEnabled",L"ServiceBellVolumePercent","service_bell");}
inline void IndependentBell(){Play(0x803,"Pull up a chair Service.esp",L".\\Data\\SKSE\\Plugins\\PullUpAChairService.ini",L"ServiceBellEnabled",L"ServiceBellVolumePercent","service_bell");}
}
