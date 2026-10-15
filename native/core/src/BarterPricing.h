#pragma once
#include <RE/Skyrim.h>
namespace inn {
int BuyCost(RE::TESBoundObject* object,RE::Actor* merchant);
void LoadMenuLayoutSettings();
extern float menuBarFraction,menuScale;
}
