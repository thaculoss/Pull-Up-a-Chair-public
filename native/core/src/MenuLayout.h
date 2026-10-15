#pragma once
#include <algorithm>
#include <cmath>
namespace inn {
struct MenuFrame {float x{},y{},width{},height{},scale{},safeTop{},safeBottom{};};
inline MenuFrame FitMenu(float screenWidth,float screenHeight,float bars=0.12f,float requestedScale=1.f,
                        float baseWidth=1120,float baseHeight=730,float offsetX=0,float offsetY=0) {
    if(!std::isfinite(bars))bars=.12f;
    if(!std::isfinite(requestedScale))requestedScale=1.f;
    bars=std::clamp(bars,0.f,.4f);
    const float safeTop=screenHeight*bars,safeBottom=screenHeight*(1-bars);
    const float scale=std::max(0.f,std::min({std::min(screenWidth/1920.f,screenHeight/1080.f)*std::clamp(requestedScale,.5f,1.4f),
        (screenWidth-24.f)/baseWidth,(safeBottom-safeTop-24.f)/baseHeight}));
    const float width=baseWidth*scale,height=baseHeight*scale;
    const float x=std::clamp((screenWidth-width)/2+offsetX*scale,12.f,std::max(12.f,screenWidth-width-12));
    const float y=std::clamp((safeTop+safeBottom-height)/2+offsetY*scale,safeTop+12,std::max(safeTop+12,safeBottom-height-12));
    return {x,y,width,height,scale,safeTop,safeBottom};
}
}
