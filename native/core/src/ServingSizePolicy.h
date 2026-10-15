#pragma once
#include <algorithm>
#include <cmath>
namespace inn::serving {
struct Settings {bool compactMain{};float targetDiameter{14.f};bool compactDesserts{};float targetDessertDiameter{14.f};};
inline bool ValidDiameter(float value){return std::isfinite(value)&&value>=8.f&&value<=40.f;}
inline bool Valid(Settings value){return ValidDiameter(value.targetDiameter)&&ValidDiameter(value.targetDessertDiameter);}
inline constexpr float plateHalfWidth=15.67728f;
inline float Fit(float halfWidth,float halfDepth,bool hasPlate,float plateScale,bool enabled,float diameter) {
    if(!enabled||!ValidDiameter(diameter)||!std::isfinite(halfWidth)||!std::isfinite(halfDepth)||halfWidth<=0||halfDepth<=0)return 1.f;
    const float limit=diameter*.5f;
    float scale=std::min({1.f,limit/halfWidth,limit/halfDepth});
    if(hasPlate) {
        if(!std::isfinite(plateScale)||plateScale<=0)return 1.f;
        // Retain the placement solver's fixed 0.5-unit plate margin after scaling.
        scale=std::min(scale,(limit-.5f)/(plateHalfWidth*plateScale));
    }
    return scale>0&&std::isfinite(scale)?scale:1.f;
}
inline float MainScale(float halfWidth,float halfDepth,bool hasPlate,float plateScale,Settings settings) {
    return Fit(halfWidth,halfDepth,hasPlate,plateScale,settings.compactMain,settings.targetDiameter);
}
inline float CourseScale(int course,float halfWidth,float halfDepth,bool hasPlate,float plateScale,Settings settings) {
    if(course==0)return MainScale(halfWidth,halfDepth,hasPlate,plateScale,settings);
    if(course==2)return Fit(halfWidth,halfDepth,false,1,settings.compactDesserts,settings.targetDessertDiameter);
    return 1.f;
}
}
