#pragma once
#include <RE/Skyrim.h>
namespace inn {
RE::BGSLocation* FindInn(RE::TESObjectREFR* reference);
bool IsSeated(RE::PlayerCharacter* player);
void LoadLanguage();
bool SaveLanguage(bool chinese);
}
