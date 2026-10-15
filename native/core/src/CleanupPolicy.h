#pragma once
namespace inn::cleanup {
inline bool Eligible(bool allItems,bool ordinaryItem,bool tabletopObject,bool fixture) {
    return !fixture&&(allItems?tabletopObject:ordinaryItem);
}
}
