#pragma once
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <vector>
namespace inn::tables {
void Install();
int NextOrder();
int RecoveredStatus(int token);
void Save(SKSE::SerializationInterface* io);
void Load(SKSE::SerializationInterface* io);
void Revert();
void Recover();
bool Track(int token, RE::TESObjectREFR* reference, RE::TESObjectREFR* seat,
           RE::TESObjectREFR* table, RE::BGSLocation* location, int kind);
bool Prepared(int token,const std::vector<RE::FormID>& foods,bool plate);
void Commit(int token);
void Cancel(int token);
void RequestClean(RE::Actor* server);
void RestoreTable(RE::Actor* server);
bool Managed(RE::FormID reference);
bool HasCommittedForTable(RE::FormID seat,RE::FormID cell,const std::vector<RE::FormID>& tables);
void Discard(RE::FormID reference);
}
