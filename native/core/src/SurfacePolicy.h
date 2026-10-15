#pragma once
#include <array>
#include <cmath>
#include <algorithm>
#include <string>
#include <string_view>
#include <cstdint>
#include <cctype>
#include "TableGeometry.h"
namespace inn {
struct Point {float x{},y{},z{};};
struct Surface {
    std::uint32_t reference{};
    int profile{};
    Point origin;
    float yaw{},scale{1};
    std::array<float,5> bounds{};
    SurfaceShape shape{SurfaceShape::rectangle};
    bool counter{};
    std::uint32_t segment{}; // 0 for a standalone reference; stable region ID inside a composite mesh.
    bool IsCounter() const{return counter||(profile>=25&&profile<35);}
    Point Local(Point point) const {
        const auto dx=point.x-origin.x,dy=point.y-origin.y;
        return {(dx*std::cos(yaw)-dy*std::sin(yaw))/scale,(dx*std::sin(yaw)+dy*std::cos(yaw))/scale,(point.z-origin.z)/scale};
    }
    float Top() const{return origin.z+bounds[4]*scale;}
    bool ContainsXY(Point point,float tolerance=1.f) const {
        if(scale<=0||!std::isfinite(scale))return false;
        const auto p=Local(point);const auto gap=tolerance/scale;
        if(p.x<bounds[0]-gap||p.x>bounds[2]+gap||p.y<bounds[1]-gap||p.y>bounds[3]+gap)return false;
        if(shape==SurfaceShape::ellipse) {
            const float x=(p.x-(bounds[0]+bounds[2])*.5f)/((bounds[2]-bounds[0])*.5f+gap);
            const float y=(p.y-(bounds[1]+bounds[3])*.5f)/((bounds[3]-bounds[1])*.5f+gap);
            return x*x+y*y<=1.f+1e-6f;
        }
        return profile!=33||p.x<=32+gap||p.y>=-32-gap;
    }
    float EdgeDistance(Point point) const {
        const auto p=Local(point);
        if(shape==SurfaceShape::ellipse) {
            const float cx=(bounds[0]+bounds[2])*.5f,cy=(bounds[1]+bounds[3])*.5f;
            const float rx=(bounds[2]-bounds[0])*.5f,ry=(bounds[3]-bounds[1])*.5f;
            const float length=std::hypot((p.x-cx)/rx,(p.y-cy)/ry);
            if(length<=1)return 0;
            return std::hypot((p.x-cx)*(1-1/length),(p.y-cy)*(1-1/length))*scale;
        }
        const auto dx=std::max({bounds[0]-p.x,0.f,p.x-bounds[2]});
        const auto dy=std::max({bounds[1]-p.y,0.f,p.y-bounds[3]});
        return std::hypot(dx,dy)*scale;
    }
};
inline bool OnSurface(const Surface& surface,Point center,float itemBottom,float itemScale=1.f) {
    if(!std::isfinite(center.x)||!std::isfinite(center.y)||!std::isfinite(center.z)||!std::isfinite(itemBottom)||!std::isfinite(itemScale))return false;
    const auto bottom=center.z+itemBottom*itemScale;
    return surface.ContainsXY(center)&&bottom>=surface.Top()-8.f&&bottom<=surface.Top()+18.f;
}
inline bool ConnectedCounters(const Surface& a,const Surface& b) {
    if(!a.IsCounter()||!b.IsCounter()||a.shape!=SurfaceShape::rectangle||b.shape!=SurfaceShape::rectangle||std::abs(a.Top()-b.Top())>1.f)return false;
    const auto angle=b.yaw-a.yaw;
    if(std::abs(std::sin(angle))>.001f&&std::abs(std::cos(angle))>.001f)return false;
    const auto c=a.Local(b.origin);const auto ratio=b.scale/a.scale;
    const auto sn=std::sin(angle),cs=std::cos(angle);
    const auto x=c.x+ratio*((b.bounds[0]+b.bounds[2])*cs+(b.bounds[1]+b.bounds[3])*sn)*.5f;
    const auto y=c.y+ratio*(-(b.bounds[0]+b.bounds[2])*sn+(b.bounds[1]+b.bounds[3])*cs)*.5f;
    const auto hx=ratio*((b.bounds[2]-b.bounds[0])*std::abs(cs)+(b.bounds[3]-b.bounds[1])*std::abs(sn))*.5f;
    const auto hy=ratio*((b.bounds[2]-b.bounds[0])*std::abs(sn)+(b.bounds[3]-b.bounds[1])*std::abs(cs))*.5f;
    const auto overlapX=std::min(a.bounds[2],x+hx)-std::max(a.bounds[0],x-hx);
    const auto overlapY=std::min(a.bounds[3],y+hy)-std::max(a.bounds[1],y-hy);
    const auto gap=2.f/a.scale;
    return (overlapX>0&&std::min(std::abs(y-hy-a.bounds[3]),std::abs(y+hy-a.bounds[1]))<gap)||
           (overlapY>0&&std::min(std::abs(x-hx-a.bounds[2]),std::abs(x+hx-a.bounds[0]))<gap);
}
inline bool DrinkwareModel(std::string_view path) {
    const auto start=path.find_last_of("/\\");const auto original=path.substr(start==std::string::npos?0:start+1);
    std::string name(original);std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return char(std::tolower(c));});
    for(const auto word:{"table","cabinet","shelf","cupboard"})if(name.find(word)!=std::string::npos)return false;
    for(const std::string_view word:{"cup","goblet","mug","tankard","chalice"}) {
        for(auto at=name.find(word);at!=std::string::npos;at=name.find(word,at+1)) {
            const auto end=at+word.size();
            // Match GlazedCup01, Cup_large and custom CupLarge models, without
            // treating Cupboard, Cupcake or Smuggler containers as drinkware.
            if(end==name.size()||!std::isalpha(static_cast<unsigned char>(name[end]))||
               std::isupper(static_cast<unsigned char>(original[end])))return true;
        }
    }
    return false;
}
inline bool TablewareModel(std::string_view path) {
    std::string model(path);std::transform(model.begin(),model.end(),model.begin(),[](unsigned char c){return char(std::tolower(c));});
    const auto start=model.find_last_of("/\\");const auto name=model.substr(start==std::string::npos?0:start+1);
    if(name.find("table")!=std::string::npos||name.find("cabinet")!=std::string::npos||name.find("shelf")!=std::string::npos||name.find("cupboard")!=std::string::npos)return false;
    for(const auto word:{"plate","platter","bowl","dish"})if(name.find(word)!=std::string::npos)return true;
    return DrinkwareModel(path);
}
inline bool UtensilModel(std::string_view path) {
    const auto model=TableModelKey(path);const auto name=model.substr(model.find_last_of('\\')+1);
    for(const auto bad:{"weapons\\","armor\\","fossil","pitchfork","spoonwing","drawknife","ruinsknife","cabinet","shelf","cupboard","table"})if(model.find(bad)!=std::string::npos)return false;
    for(const auto word:{"spoon","fork","knife","ladle","spork","chopstick"})if(name.find(word)!=std::string::npos)return true;
    return false;
}
inline bool DiningWeaponModel(std::string_view path) {
    const auto model=TableModelKey(path);
    return model.ends_with("clutter\\dining set\\basicknife01.nif")||model.ends_with("clutter\\dining set\\basicfork01.nif");
}
inline bool BeverageBottleModel(std::string_view path) {
    const auto model=TableModelKey(path);const auto name=model.substr(model.find_last_of('\\')+1);
    for(const auto bad:{"table","cabinet","shelf","cupboard","rack","basket","crate","stand","barrel","keg","cask","vat"})if(name.find(bad)!=std::string::npos)return false;
    // Solitude's empty spiced-wine bottle has no WineBottle prefix (Skyrim.esm:0FED17).
    if(name=="winesanspicedwineempty01.nif")return true;
    if(name.find("winebottle")!=std::string::npos||name.find("meadbottle")!=std::string::npos||name.find("beerbottle")!=std::string::npos||name.find("alebottle")!=std::string::npos)return true;
    return name.starts_with("mead")||name.starts_with("nordmead")||name.starts_with("blackbriarmead")||name.starts_with("wine01")||name.starts_with("wine02");
}
inline bool BulkContainerModel(std::string_view path) {
    const auto model=TableModelKey(path);const auto name=model.substr(model.find_last_of('\\')+1);
    return name.find("barrel")!=std::string::npos||name.find("keg")!=std::string::npos||name.find("cask")!=std::string::npos;
}
inline bool FoodDisplayModel(std::string_view path) {
    const auto model=TableModelKey(path);const auto name=model.substr(model.find_last_of('\\')+1);
    if(model.find("food\\")==std::string::npos&&model.find("ingredients\\")==std::string::npos)return false;
    for(const auto bad:{"table","cabinet","shelf","cupboard"})if(name.find(bad)!=std::string::npos)return false;
    for(const auto word:{"dumpling","jiaozi","gyoza","pierogi","wonton","baozi","sweetroll","bread","cheese","pie","tart","pastry","crostat","apple","salmon","stew","soup"})if(name.find(word)!=std::string::npos)return true;
    return false;
}
inline bool FixedStructureModel(std::string_view path) {
    const auto model=TableModelKey(path);const auto name=model.substr(model.find_last_of('\\')+1);
    for(const auto word:{"interior","exterior","ceiling","floor","wall","pillar","column","beam","stairs","stairway","door","window","railing","cabinet","cupboard","shelf","fireplace"})
        if(name.find(word)!=std::string::npos)return true;
    return false;
}
inline bool RemovePlate(bool seated,int remainingFood){return !seated||remainingFood==0;}
}
