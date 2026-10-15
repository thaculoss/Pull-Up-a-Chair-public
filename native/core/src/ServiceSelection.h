#pragma once
#include "ServicePolicy.h"
#include <algorithm>
#include <span>
#include <tuple>
namespace inn::service::selection {
enum class Role { none,waiter,owner };
struct Candidate {std::uint32_t id{};Role role{Role::none};float distanceSquared{};bool available{};};
inline constexpr unsigned attemptLimit=8;
inline constexpr float callTimeout=60.f;
inline const Candidate* Choose(std::span<const Candidate> candidates,std::span<const std::uint32_t> attempted={}){
    const Candidate* chosen=nullptr;
    for(const auto& candidate:candidates){
        if(!candidate.id||candidate.role==Role::none||!candidate.available||
           !std::isfinite(candidate.distanceSquared)||candidate.distanceSquared<0||
           std::ranges::find(attempted,candidate.id)!=attempted.end())continue;
        const auto rank=[](const Candidate& value){return std::tuple(value.role==Role::waiter?0:1,value.distanceSquared,value.id);};
        if(!chosen||rank(candidate)<rank(*chosen))chosen=&candidate;
    }
    return chosen;
}
inline bool CanFallback(bool called,Phase phase,unsigned attempts,float elapsed){
    return called&&(phase==Phase::preparing||phase==Phase::approaching)&&
        attempts<attemptLimit&&std::isfinite(elapsed)&&elapsed>=0&&elapsed<callTimeout;
}
}
