#pragma once
#include <cstdint>
namespace innservice {
inline constexpr std::uint32_t supportedPad=0x3F3FF;
inline std::uint32_t PadButton(std::uint32_t raw) {
    if(raw==9)return 0x10000; // LT has a non-bitmask engine ID.
    if(raw==10)return 0x20000; // RT
    return raw&&!(raw&(raw-1))&&(raw&0xF3FF)?raw:0;
}
enum class CaptureDevice {none,keyboard,gamepad};
enum class CaptureAction {none,complete,cancelled};
struct CaptureResult {CaptureAction action{};CaptureDevice device{};std::uint32_t binding{};};
struct BindingRecorder {
    CaptureDevice mode{CaptureDevice::none};
    std::uint32_t held{},candidate{};
    void Begin(CaptureDevice device){mode=device;held=candidate=0;}
    void Cancel(){mode=CaptureDevice::none;held=candidate=0;}
    CaptureResult Event(CaptureDevice device,std::uint32_t raw,bool down,bool up) {
        if(mode==CaptureDevice::none)return {};
        if(device==CaptureDevice::keyboard&&raw==1&&down){Cancel();return {CaptureAction::cancelled};}
        if(device!=mode)return {};
        if(mode==CaptureDevice::keyboard) {
            if(down&&raw>1&&raw<=255&&!candidate){held=candidate=raw;}
            if(up&&candidate&&raw==held){const auto value=candidate;Cancel();return {CaptureAction::complete,CaptureDevice::keyboard,value};}
        }else {
            const auto button=PadButton(raw);if(!button)return {};
            if(down){held|=button;candidate|=button;}
            if(up){held&=~button;if(candidate&&!held){const auto value=candidate;Cancel();return {CaptureAction::complete,CaptureDevice::gamepad,value};}}
        }
        return {};
    }
};
}
