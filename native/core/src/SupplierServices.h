#pragma once
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include "SupplierPolicy.h"
namespace inn::suppliers {
inline constexpr std::uint32_t recordType=0x4953504C;
struct Service {Identity id;RE::Actor* merchant{};RE::TESObjectREFR* stock{};};
void Initialize();
Service Resolve(RE::Actor* server);
void Save(SKSE::SerializationInterface* io);
void Load(SKSE::SerializationInterface* io,std::uint32_t version,std::uint32_t length);
void Revert();
}
