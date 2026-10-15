#pragma once
#include <cstdint>
#include <span>
#include <tuple>
namespace inn::suppliers {
// Identity is attached to an inn, never to the person taking an order or their distance.
struct Identity {
    std::uint32_t inn{},merchant{},faction{},stock{},priority{};
    auto operator<=>(const Identity&) const = default;
};
static_assert(sizeof(Identity)==20);
struct Candidate {Identity id;bool alive{},enabled{},innkeeper{},member{},hasStock{};};
inline bool BelongsToInn(std::uint32_t inn,std::uint32_t home,std::uint32_t stockHome) {
    return inn&&((home==inn&&(!stockHome||stockHome==inn))||stockHome==inn);
}
inline bool HasAuthority(bool hasRole,std::uint32_t roleOwner,std::uint32_t merchant,bool factionMember) {
    return hasRole?(merchant&&roleOwner==merchant):factionMember;
}
inline const Candidate* Select(std::uint32_t inn,std::span<const Candidate> candidates) {
    const Candidate* best=nullptr;
    for(const auto& row:candidates) {
        if(row.id.inn!=inn||!row.id.merchant||!row.id.faction||!row.alive||!row.enabled||!row.innkeeper||!row.member)continue;
        const auto rank=[](const Candidate& c){return std::tuple(!c.hasStock,c.id.priority,c.id.faction,c.id.merchant);};
        if(!best||rank(row)<rank(*best))best=&row;
    }
    return best;
}
inline bool SameSupplier(const Identity& quoted,const Identity& current,bool needsStock) {
    return quoted.inn&&quoted.inn==current.inn&&quoted.merchant&&quoted.merchant==current.merchant&&
        quoted.faction==current.faction&&(!needsStock||(quoted.stock&&quoted.stock==current.stock));
}
}
