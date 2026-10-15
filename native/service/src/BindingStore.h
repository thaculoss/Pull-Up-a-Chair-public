#pragma once
#include <windows.h>
#include "BindingPolicy.h"
#include <algorithm>
#include <string>
namespace innservice {
struct BindingValues {int keyboard{37};std::uint32_t gamepad{0x300};};
inline BindingValues ReadBindings(const char* defaults,const char* user) {
    BindingValues value;
    value.keyboard=int(GetPrivateProfileIntA("Input","KeyboardKey",37,defaults));
    value.gamepad=GetPrivateProfileIntA("Input","GamepadChord",768,defaults);
    value.keyboard=std::clamp(int(GetPrivateProfileIntA("Input","KeyboardKey",value.keyboard,user)),0,255);
    value.gamepad=GetPrivateProfileIntA("Input","GamepadChord",value.gamepad,user)&supportedPad;
    return value;
}
inline bool WriteKeyboardBinding(const char* user,int key) {
    if(key<0||key>255||key==1)return false;
    const auto value=std::to_string(key);
    return WritePrivateProfileStringA("Input","KeyboardKey",value.c_str(),user)!=FALSE;
}
inline bool WriteGamepadBinding(const char* user,std::uint32_t mask) {
    if(mask&~supportedPad)return false;
    const auto value=std::to_string(mask);
    return WritePrivateProfileStringA("Input","GamepadChord",value.c_str(),user)!=FALSE;
}
}
