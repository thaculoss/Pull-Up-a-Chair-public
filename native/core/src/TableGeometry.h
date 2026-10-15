#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <string>
#include <string_view>
namespace inn {
enum class SurfaceShape { rectangle, ellipse };
inline std::string TableModelKey(std::string_view model) {
    std::string key(model);
    std::transform(key.begin(),key.end(),key.begin(),[](unsigned char c){return c=='/'?'\\':char(std::tolower(c));});
    while(key.starts_with(".\\"))key.erase(0,2);
    if(key.starts_with("meshes\\"))key.erase(0,7);
    return key;
}
inline std::array<float,5> PlacementBounds(std::array<float,5> bounds,SurfaceShape shape) {
    if(shape==SurfaceShape::ellipse) {
        const float cx=(bounds[0]+bounds[2])*.5f,cy=(bounds[1]+bounds[3])*.5f;
        const float hx=(bounds[2]-bounds[0])*.5f*.70710678f,hy=(bounds[3]-bounds[1])*.5f*.70710678f;
        bounds[0]=cx-hx;bounds[2]=cx+hx;bounds[1]=cy-hy;bounds[3]=cy+hy;
    }
    return bounds;
}
inline std::array<float,2> RotatedSupport(float hx,float hy,float angle) {
    const float sn=std::abs(std::sin(angle)),cs=std::abs(std::cos(angle));
    const float outerX=hx*cs+hy*sn,outerY=hx*sn+hy*cs;
    const float factor=std::min(hx/(outerX*cs+outerY*sn),hy/(outerX*sn+outerY*cs));
    return {outerX*factor,outerY*factor};
}
}
