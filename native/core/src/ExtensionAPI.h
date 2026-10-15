#pragma once

// 中文阅读说明
// 点餐核心发给可选扩展的消息定义：上菜登记、食用开始和结束、餐盘归属。
// 阅读重点：消息中的编号对应游戏引用或基础物品；接收方按消息类型解释数据。

#include <cstdint>

namespace inn::extension
{
    inline constexpr std::uint32_t registerMeal = 0x494F0001;
    inline constexpr std::uint32_t consumeBegin = 0x494F0002;
    inline constexpr std::uint32_t consumeEnd = 0x494F0003;
    inline constexpr std::uint32_t orderPlate = 0x494F0004;

    struct Meal
    {
        std::uint32_t food{}, plate{}, base{};
    };
}
