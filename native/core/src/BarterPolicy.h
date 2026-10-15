#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
namespace inn {
struct BarterInputs {
    float speech{},fortify{},minFactor{2.0f},maxFactor{3.3f},minBuyFactor{1.05f};
};
inline float SpeechFactor(const BarterInputs& input) {
    if(!std::isfinite(input.speech)||!std::isfinite(input.fortify)||!std::isfinite(input.minFactor)||
       !std::isfinite(input.maxFactor)||!std::isfinite(input.minBuyFactor)||input.minFactor<=0||input.maxFactor<=0||input.minBuyFactor<0)return -1;
    const auto factor=input.maxFactor-(input.maxFactor-input.minFactor)*std::clamp(input.speech,0.f,100.f)/100.f;
    return factor*(1.f-std::clamp(input.fortify,0.f,100.f)/100.f);
}
inline int BuyPrice(int base,float modifiedFactor,float minBuyFactor) {
    if(base<0||!std::isfinite(modifiedFactor)||!std::isfinite(minBuyFactor)||modifiedFactor<0||minBuyFactor<0)return -1;
    if(!base)return 0;
    const double value=double(base)*std::max(modifiedFactor,minBuyFactor);
    if(!std::isfinite(value)||value>std::numeric_limits<int>::max())return -1;
    return std::max(1,static_cast<int>(std::floor(value+.5)));
}
}
