#pragma once
#include "PCH.h"
namespace inn::service::priority {
enum class Result { ready,busy,missing,invalid,transition };
RE::BGSScene* Scene(RE::Actor* actor);
bool AmbientScene(RE::BGSScene* scene);
bool CanPrepare(RE::Actor* actor,bool prioritize=true);
bool StopAmbient(RE::Actor* actor);
void ReserveForService(RE::Actor* actor);
void ReportCompatibility();
bool ResetBefore(RE::Actor* actor);
Result Acquire(RE::Actor* actor,bool hold=false);
void Release(RE::Actor* actor,bool reset=true);
bool Owns(RE::Actor* actor);
bool NeedsResume(RE::Actor* actor);
bool RecheckOwnedTravel(RE::Actor* actor);
void Trace(RE::Actor* actor,const char* reason);
}
