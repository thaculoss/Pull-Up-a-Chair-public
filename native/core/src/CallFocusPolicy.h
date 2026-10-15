#pragma once
#include "SurfacePolicy.h"
#include "CallPolicy.h"
#include <optional>
#include <span>
#include <string>
namespace inn::call::focus {
enum class PromptKind {hidden,table,compatible};
// The compatibility hint follows the real DAK input decision, including the
// valid empty-crosshair/own-seat cases that need no tabletop camera hit.
inline PromptKind ChoosePrompt(bool compatibility,bool enabled,bool cameraTable,bool canDispatch) {
    if(!enabled)return PromptKind::hidden;
    if(compatibility)return canDispatch?PromptKind::compatible:PromptKind::hidden;
    return cameraTable?PromptKind::table:PromptKind::hidden;
}
// Game-thread request state. The UI thread acknowledges this generation only
// after our text field is absent; a queued hide is not a completed hide.
struct PromptPublication {
    std::string text;
    std::uint64_t generation{1},lastQueued{};
    bool queued{};
    bool Request(std::string_view desired,std::uint64_t now,std::uint64_t hiddenGeneration) {
        const bool changed=text!=desired;
        if(changed){text=desired;++generation;}
        if(!changed) {
            if(text.empty()&&hiddenGeneration==generation)return false;
            if(queued&&now>=lastQueued&&now-lastQueued<250)return false;
        }
        lastQueued=now;queued=true;return true;
    }
    void Invalidate(){++generation;queued=false;}
};
inline bool Enabled(Settings settings,bool frameworkAvailable,bool held,const Context& context) {
    return settings.mode==Mode::framework&&frameworkAvailable&&held&&context.seated&&context.inInn&&!context.inCombat&&!context.menuOpen;
}
struct Hit {std::uint32_t reference{};Point point;float distance{};};
inline float DistanceSquared(Point a,Point b){const auto x=a.x-b.x,y=a.y-b.y,z=a.z-b.z;return x*x+y*y+z*z;}
inline std::optional<Hit> PickTable(std::span<const Surface> surfaces,Point seat,Point origin,Point direction) {
    const float length=std::sqrt(DistanceSquared(direction,{}));
    if(!std::isfinite(length)||length<.001f)return {};
    direction={direction.x/length,direction.y/length,direction.z/length};
    if(direction.z>=-.001f)return {}; // Only the upper surface, never through its underside.
    std::optional<Hit> result;
    for(const auto& surface:surfaces) {
        if(!surface.reference||surface.EdgeDistance(seat)>180.f)continue;
        const float distance=(surface.Top()-origin.z)/direction.z;
        if(!std::isfinite(distance)||distance<0||distance>1200.f)continue;
        const Point point{origin.x+direction.x*distance,origin.y+direction.y*distance,surface.Top()};
        if(!surface.ContainsXY(point,0)||DistanceSquared(point,seat)>240.f*240.f)continue;
        if(!result||distance<result->distance)result=Hit{surface.reference,point,distance};
    }
    return result;
}
inline bool Unobstructed(const Hit& table,bool collided,std::uint32_t hitReference,float hitDistance) {
    // A direct food/NPC/wall hit always wins over the table behind it.
    // A registered plane without collision is still useful for composite meshes.
    return !collided||(hitReference==table.reference&&std::abs(hitDistance-table.distance)<=14.f);
}
inline bool Fresh(std::uint32_t seat,std::uint32_t cell,std::uint32_t currentSeat,std::uint32_t currentCell,
                  std::uint64_t timestamp,std::uint64_t now) {
    return seat&&cell&&seat==currentSeat&&cell==currentCell&&now>=timestamp&&now-timestamp<=150;
}
}
