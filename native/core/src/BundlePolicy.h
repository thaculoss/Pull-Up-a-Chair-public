#pragma once
#include "DailyPolicy.h"
namespace inn {
inline constexpr std::array menuTabs{3,0,1,2}; // House sets, main, drink, dessert/side.
inline int MoveMenuTab(int current,int direction) {
    auto at=std::find(menuTabs.begin(),menuTabs.end(),current);
    int index=at==menuTabs.end()?0:int(at-menuTabs.begin());
    return menuTabs[(index+direction+int(menuTabs.size()))%menuTabs.size()];
}
inline Quote BundlePrice(const std::array<Id,3>& dishes,const std::map<Id,Offer>& offers,bool daily) {
    return MenuPrice(dishes,offers,daily,dishes);
}
inline bool BundleSelected(const std::array<Id,3>& selected,bool dailyMode,
                           const std::array<Id,3>& dishes,bool daily) {
    return std::any_of(dishes.begin(),dishes.end(),[](auto id){return id!=0;})&&selected==dishes&&dailyMode==daily;
}
inline bool ToggleBundle(std::array<Id,3>& selected,bool& dailyMode,
                         const std::array<Id,3>& dishes,bool daily,const std::map<Id,Offer>& offers) {
    if(!BundlePrice(dishes,offers,daily).valid)return false;
    if(BundleSelected(selected,dailyMode,dishes,daily)){selected={};dailyMode=false;}
    else {selected=dishes;dailyMode=daily;}
    return true;
}
}
