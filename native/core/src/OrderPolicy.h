#pragma once
#include <array>
#include <cstdint>
#include <limits>
#include <map>
#include <algorithm>
namespace inn {
using Id = std::uint64_t;
inline constexpr Id generatedBit=Id{1}<<32;
inline constexpr Id SourceKey(std::uint32_t form,bool generated=false){return Id{form}|(generated?generatedBit:0);}
inline constexpr bool Generated(Id key){return (key&generatedBit)!=0;}
inline constexpr std::uint32_t BaseForm(Id key){return static_cast<std::uint32_t>(key);}
template<class Row> Id RowKey(const Row& row){return SourceKey(row.id,row.generated);}
// Authored regional food remains in the offer snapshot for the house bundle,
// but only merchant-stock dishes appear in individual course browsing.
template<class Row> int BrowseMask(const Row& row){return row.generated?0:row.menuMask>=0?row.menuMask:row.mask;}
template<class Row> int SelectionSlot(const Row& row,int tab) {
    return tab>=0&&tab<3&&(row.mask&(1<<tab))?tab:row.course;
}
template<class Row> bool RowSelected(const std::array<Id,3>& selected,const Row& row,int tab) {
    const auto key=RowKey(row);
    if(row.generated)return std::find(selected.begin(),selected.end(),key)!=selected.end();
    const auto slot=SelectionSlot(row,tab);return slot>=0&&slot<3&&selected[slot]==key;
}
template<class Row> void ToggleRow(std::array<Id,3>& selected,const Row& row,int tab) {
    const auto slot=SelectionSlot(row,tab);if(slot<0||slot>=3)return;
    const auto key=RowKey(row);const bool chosen=RowSelected(selected,row,tab);
    // A fixed specialty combo can put a soup in its authored main slot.
    // Cancel it there before single-item selection uses the side slot.
    if(row.generated)for(auto& id:selected)if(id==key)id=0;
    if(!chosen)selected[slot]=key;else if(!row.generated)selected[slot]=0;
}
struct Offer { int stock{}, price{}, mask{}; bool generated{}; };
struct Quote {
    bool valid{};
    int total{};
    std::map<Id, int> quantities;
    std::map<Id, int> inventoryQuantities;
};
inline Quote Price(const std::array<Id,3>& selected, const std::map<Id,Offer>& offers) {
    Quote result;
    std::int64_t total = 0;
    for (int slot = 0; slot < 3; ++slot) {
        const auto id = selected[slot];
        if (!id) continue;
        const auto offer = offers.find(id);
        if (offer == offers.end() || !(offer->second.mask & (1 << slot)) || offer->second.price < 0 || offer->second.generated!=Generated(id)) return {};
        ++result.quantities[id];
        if(!offer->second.generated)++result.inventoryQuantities[id];
        total += offer->second.price;
    }
    if (result.quantities.empty() || total > std::numeric_limits<int>::max()) return {};
    for (const auto& [id, count] : result.inventoryQuantities) if (offers.at(id).stock < count) return {};
    result.valid = true;
    result.total = static_cast<int>(total);
    return result;
}
inline bool CanPurchase(const Quote& quote, int gold, bool sameContext) {
    return quote.valid && sameContext && gold >= quote.total;
}
}
