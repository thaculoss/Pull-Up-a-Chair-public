#pragma once
#include "PCH.h"
namespace inn::service {
inline constexpr std::uint32_t recordType=0x49534150;
bool Request();
bool Available();
bool Busy();
bool CanRepeatCall();
bool CanWalkForService(RE::Actor* server);
// Core readiness includes completion of legacy alias migration, when installed.
bool BackendReady();
void BackendDataLoaded(bool ready);
std::uint64_t Generation();
void Update();
void OrderStarted(RE::Actor* server,std::uint32_t token);
void OrderFinished(RE::Actor* server,std::uint32_t token,bool committed);
void Reset();
void PrepareLoad();
void Save(SKSE::SerializationInterface* io);
void Load(SKSE::SerializationInterface* io,std::uint32_t version,std::uint32_t length);
void Recover();
}
