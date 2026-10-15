#pragma once
#include "InnServiceAPI.h"
#include <cmath>
#include <map>
#include <tuple>
#include <vector>
#include <algorithm>
namespace innservice {
inline bool RepeatBellAllowed(std::uint32_t phase){return phase==static_cast<std::uint32_t>(api::Phase::starting)||phase==static_cast<std::uint32_t>(api::Phase::approaching)||phase==static_cast<std::uint32_t>(api::Phase::waiting);}
enum class Role { none,owner,waiter };
struct Staff {
    std::uint32_t id{},base{},inn{},cell{},faction{},chest{};
    Role role{Role::none};
    float distance{};
    bool available{};
};
inline std::uint32_t ChooseServer(const std::vector<Staff>& staff,std::uint32_t inn,std::uint32_t cell,float radius) {
    const Staff* chosen=nullptr;
    for(const auto& member:staff)if(member.available&&member.role!=Role::none&&member.inn==inn&&member.cell==cell&&
        std::isfinite(member.distance)&&member.distance>=0&&member.distance<=radius) {
        auto rank=[](const Staff& a){return std::tuple(a.role==Role::waiter?0:1,a.distance,a.id);};
        if(!chosen||rank(member)<rank(*chosen))chosen=&member;
    }
    return chosen?chosen->id:0;
}
struct Vendor {std::uint32_t inn{},faction{},chest{},merchant{};bool food{},owner{};};
inline api::Supply ChooseSupply(std::uint32_t inn,const std::vector<Vendor>& vendors,std::uint32_t staffCount) {
    api::Supply result;result.inn=inn;result.staffCount=staffCount;
    std::vector<Vendor> eligible;
    for(auto vendor:vendors)if(vendor.inn==inn&&vendor.chest&&(vendor.owner||vendor.food))eligible.push_back(vendor);
    if(eligible.empty())return result;
    auto rank=[](const Vendor& v){return std::tuple(v.owner?0:1,v.merchant?0:1,v.chest,v.faction,v.merchant);};
    std::sort(eligible.begin(),eligible.end(),[&](const auto& a,const auto& b){return rank(a)<rank(b);});
    const auto chosen=eligible.front();
    // Without an owner, two distinct eligible food chests are ambiguous. Do not guess another shop.
    if(!chosen.owner)for(const auto& v:eligible)if(v.chest!=chosen.chest){result.status=static_cast<std::uint32_t>(api::Status::ambiguousStock);return result;}
    result.chest=chosen.chest;result.faction=chosen.faction;result.merchant=chosen.merchant;
    result.status=static_cast<std::uint32_t>(chosen.merchant?api::Status::ready:api::Status::stockOnly);
    return result;
}
struct Context {bool seated{},sameSeat{},sameCell{},sameInn{},combat{},staffValid{};};
inline bool Continues(const Context& c){return c.seated&&c.sameSeat&&c.sameCell&&c.sameInn&&!c.combat&&c.staffValid;}
inline bool Arrived(float horizontal,float vertical,float range){return std::isfinite(horizontal)&&std::isfinite(vertical)&&horizontal<=range&&vertical<=80;}
inline bool Expired(float elapsed,float limit){return !std::isfinite(elapsed)||elapsed>=limit;}
struct PadChord {
    std::uint32_t held{};
    bool Update(std::uint32_t button,bool pressed,std::uint32_t chord) {
        const bool before=chord&&(held&chord)==chord;
        if(pressed)held|=button;else held&=~button;
        return !before&&chord&&(held&chord)==chord;
    }
};
}
