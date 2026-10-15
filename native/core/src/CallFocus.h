#pragma once
#include <RE/Skyrim.h>
namespace inn::call::focus {
void Initialize();
void UpdateTarget(RE::PlayerCharacter* player,bool enabled);
void UpdatePrompt(bool enabled,bool cameraTable,bool canDispatch);
RE::NiPointer<RE::TESObjectREFR> Target(RE::PlayerCharacter* player);
void Reset();
}
