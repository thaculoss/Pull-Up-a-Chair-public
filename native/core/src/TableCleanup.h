#pragma once
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include "SurfacePolicy.h"
namespace inn::cleanup {
struct Target {RE::FormID seat{},cell{};std::vector<std::pair<RE::FormID,std::uint32_t>> regions;};
Target Capture();
void Request(RE::Actor* server,bool restore=false);
// Synchronous game-thread entry used to guarantee cleanup finishes before greeting.
bool ExecuteNow(RE::Actor* server,bool restore=false,bool notify=true,const Target* target=nullptr);
void Save(SKSE::SerializationInterface* io);
void Load(SKSE::SerializationInterface* io,std::uint32_t version,std::uint32_t length);
void Revert();
inline constexpr std::uint32_t recordType=0x494F434C;
}
