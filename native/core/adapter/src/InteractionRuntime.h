#pragma once
#include <cstddef>
namespace meal {
// Only this optional module installs the food/plate interaction hooks.
inline constexpr std::size_t activateSlot = 0x37;
inline constexpr std::size_t activateTextSlot = 0x4C;
// CommonLib v11 Actor::DrinkPotion uses a different virtual slot in VR.
constexpr std::size_t DrinkSlot(bool isVR) { return isVR ? 0x111 : 0x10F; }
} // namespace meal
