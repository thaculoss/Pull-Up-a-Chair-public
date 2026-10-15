#pragma once
#include <RE/Skyrim.h>
#include "ServicePolicy.h"
namespace innservice {
struct Resolved {api::Supply supply;std::vector<Staff> staff;};
RE::BGSLocation* FindInn(RE::TESObjectREFR* reference);
bool Seated(RE::PlayerCharacter* player);
bool Available(RE::Actor* actor,RE::PlayerCharacter* player);
Resolved Resolve(RE::TESObjectREFR* reference);
void LoadRoles();
}
