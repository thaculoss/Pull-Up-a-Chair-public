#pragma once
#include <array>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <span>
namespace inn {
struct Serving {
    std::uint32_t reference{}, base{}, cell{}, seat{}, table{}, inn{};
    std::uint32_t order{}, kind{}, committed{}; // kind: 0 first dish, 1 tableware, 2 other food
    float x{}, y{}, z{};
};
static_assert(sizeof(Serving) == 48);
inline bool PaidAtTable(const Serving& serving,std::uint32_t seat,std::uint32_t cell,std::span<const std::uint32_t> tables){
    return serving.committed&&serving.cell==cell&&
        (serving.seat==seat||std::find(tables.begin(),tables.end(),serving.table)!=tables.end());
}
inline bool CanClear(const Serving& serving, std::uint32_t cell, std::uint32_t table,
                     bool inContainer, float x, float y, float z) {
    if (!serving.committed || serving.cell != cell || !table || serving.table != table || inContainer) return false;
    // An item deliberately moved away is no longer part of the table's service.
    return std::abs(serving.x-x) < 12 && std::abs(serving.y-y) < 12 && std::abs(serving.z-z) < 12;
}
}
