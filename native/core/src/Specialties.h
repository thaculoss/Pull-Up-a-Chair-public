#pragma once
#include <RE/Skyrim.h>
#include <array>
#include <filesystem>
namespace inn::specialties {
void Load(const std::filesystem::path& folder);
std::array<RE::FormID,3> ForInn(RE::BGSLocation* location);
}
