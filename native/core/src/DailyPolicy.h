#pragma once
#include "OrderPolicy.h"
#include <algorithm>
#include <vector>
#include <functional>
namespace inn {
inline std::uint32_t CalendarDay(unsigned year,unsigned month,unsigned day) {
    return year*372+month*31+day;
}
inline std::uint64_t Mix(std::uint64_t value) {
    value+=0x9e3779b97f4a7c15ULL;
    value=(value^(value>>30))*0xbf58476d1ce4e5b9ULL;
    value=(value^(value>>27))*0x94d049bb133111ebULL;
    return value^(value>>31);
}
inline bool DailyCategoriesMatch(const std::array<Id,3>& dishes,const std::map<Id,Offer>& categories) {
    for(int slot=0;slot<3;++slot)if(auto i=categories.find(dishes[slot]);dishes[slot]&&i!=categories.end())
        if(!(i->second.mask&(1<<slot)))return false;
    return true;
}
inline std::array<Id,3> PickDaily(std::uint64_t innKey,std::uint32_t day,const std::map<Id,Offer>& offers) {
    std::array<Id,3> result{},current{};std::map<Id,int> needed;
    std::array<std::vector<Id>,3> candidates;
    for(int slot=0;slot<3;++slot) {
        for(auto [id,offer]:offers)if(!offer.generated&&offer.stock>0&&(offer.mask&(1<<slot)))candidates[slot].push_back(id);
        const auto salt=innKey^Mix(day)^Mix(slot+1);
        std::sort(candidates[slot].begin(),candidates[slot].end(),[salt](Id a,Id b){return std::pair(Mix(salt^a),a)<std::pair(Mix(salt^b),b);});
        candidates[slot].push_back(0);
    }
    int best=-1;
    std::function<void(int,int)> search=[&](int slot,int count) {
        if(best==3||count+3-slot<=best)return;
        if(slot==3){if(count>best){best=count;result=current;}return;}
        for(auto id:candidates[slot]) {
            if(id&&needed[id]>=offers.at(id).stock)continue;
            current[slot]=id;if(id)++needed[id];search(slot+1,count+(id!=0));if(id)--needed[id];
        }
    };
    search(0,0);
    return result;
}
inline Quote MenuPrice(const std::array<Id,3>& selected,const std::map<Id,Offer>& offers,
                       bool daily,const std::array<Id,3>& dailyDishes) {
    auto quote=Price(selected,offers);
    if(!quote.valid)return {};
    if(daily) {
        if(selected!=dailyDishes)return {};
        // Gold is integral: an odd total rounds upward, never to a free one-gold meal.
        quote.total=quote.total/2+quote.total%2;
    }
    return quote;
}
}
