#pragma once
#include <algorithm>
#include <cstdint>
namespace food {
// XInput button bits plus a digital dead zone for the left stick.
inline constexpr std::uint32_t directions=0xF;
struct PadRepeat {
    bool armed{};
    std::uint32_t previous{};
    double repeatAt{};
    std::uint32_t Update(std::uint32_t down,double now) {
        if(!armed){if(!down)armed=true;previous=down;return 0;}
        auto pressed=down&~previous;
        if((down&directions)!=(previous&directions))repeatAt=now+0.4;
        else if((down&directions)&&now>=repeatAt){pressed|=down&directions;repeatAt=now+0.18;}
        previous=down;return pressed;
    }
};
inline int MoveGrid(int index,int count,int columns,int dx,int dy) {
    if(count<=0)return -1;
    columns=std::max(1,columns);index=std::clamp(index,0,count-1);
    if(dy)return dy<0?std::max(0,index-columns):std::min(count-1,index+columns);
    int first=(index/columns)*columns,last=std::min(count-1,first+columns-1);
    return std::clamp(index+dx,first,last);
}
}
