#pragma once
#include "DailyPolicy.h"
#include <SKSE/SKSE.h>
namespace inn::daily {
std::uint32_t Today();
std::array<Id,3> Menu(RE::BGSLocation* location,std::uint32_t day,const std::map<Id,Offer>& offers);
void Save(SKSE::SerializationInterface* io);
void Load(SKSE::SerializationInterface* io,std::uint32_t version,std::uint32_t length);
void Revert();
inline constexpr std::uint32_t recordType=0x494F444D;
}
