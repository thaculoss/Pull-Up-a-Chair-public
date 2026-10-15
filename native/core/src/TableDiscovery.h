#pragma once
#include <RE/Skyrim.h>
#include "SurfacePolicy.h"
namespace inn::discovery {
void Initialize();
int Index(RE::TESBoundObject* base);
int ProfileAt(RE::TESObjectREFR* center,int slot);
std::vector<Surface> Surfaces(RE::TESObjectREFR* center,float radius=0);
std::vector<RE::TESObjectREFR*> Nearby(RE::TESObjectREFR* center,float radius);
}
