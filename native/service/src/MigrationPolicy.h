#pragma once
#include <algorithm>
#include <cmath>
namespace innservice {
// A slow accepted Papyrus stack must retain its ticket and remain eligible for
// completion. Never open the backend gate or enqueue a second accepted stack.
struct MigrationProgress {
    float activeSeconds{};
    bool warned{};
    bool Advance(float delta,bool sessionReady,bool blocked) {
        if(!sessionReady||blocked||!std::isfinite(delta))return false;
        activeSeconds+=std::clamp(delta,0.f,1.f);
        if(activeSeconds<20.f||warned)return false;
        warned=true;return true;
    }
};
inline bool CanMigrate(bool sessionReady,bool playerInCell,bool blocked) {
    return sessionReady&&playerInCell&&!blocked;
}
}
