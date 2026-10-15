#pragma once

// 中文阅读说明
// EAS 动画识别与餐盘保护规则：新效果、来源、脚本身份、隐藏与恢复条件。
// 阅读重点：世界中这份食物触发的新动画效果才取得票据，已有的背包效果不能混入。

#include <algorithm>
#include <cstdint>
#include <string_view>
#include <vector>

namespace meal
{
    inline bool TablewareStart(std::string_view command)
    {
        return command == "EasChairEuStart" || command == "EasChairEuStStart";
    }

    inline bool CurrentSpellObservation(std::uint64_t currentEpoch,
                                        std::uint64_t currentSequence,
                                        std::uint32_t currentFood,
                                        std::uint64_t observedEpoch,
                                        std::uint64_t observedSequence,
                                        std::uint32_t observedFood)
    {
        return currentSequence && currentFood && currentEpoch == observedEpoch &&
               currentSequence == observedSequence && currentFood == observedFood;
    }

    struct EffectKey
    {
        std::uintptr_t address{};
        std::uint32_t spell{};
        std::uint16_t serial{};
        bool operator==(const EffectKey &) const = default;
    };

    // A ticket can only be issued for a new effect made inside the activation of
    // an exactly paired world food. Existing / inventory effects never qualify.
    // 必须是本次世界食物激活中新出现的效果，才属于这次进食。
    inline bool IsNewMealEffect(const EffectKey &key,
                                std::uint32_t foodBase,
                                const std::vector<EffectKey> &before)
    {
        return key.address && key.spell == foodBase &&
               std::find(before.begin(), before.end(), key) == before.end();
    }
    enum class AnimationRoute
    {
        none,
        directFood,
        selectedSpell
    };

    // 区分直接食物效果与 EAS 选出的动画法术效果。
    inline AnimationRoute ClassifyAnimationEffect(const EffectKey &key,
                                                  std::uint32_t foodBase,
                                                  std::uint32_t selectedSpell,
                                                  const std::vector<EffectKey> &before)
    {
        if (foodBase && IsNewMealEffect(key, foodBase, before))
            return AnimationRoute::directFood;
        if (selectedSpell && IsNewMealEffect(key, selectedSpell, before))
            return AnimationRoute::selectedSpell;
        return AnimationRoute::none;
    }

    struct Ticket
    {
        EffectKey effect;
        std::uint32_t plate{};
        bool hidden{};
        double created{};
    };

    template <class Tickets> bool HiddenForAnimation(std::uint32_t plate, const Tickets &tickets)
    {
        // Whole-order ownership protects activation; it must not veto EAS visibility.
        return std::any_of(tickets.begin(),
                           tickets.end(),
                           [plate](const auto &t)
                           {
                               return t.plate == plate && t.hidden;
                           });
    }

    struct Origin
    {
        std::uint32_t food{}, plate{}, base{};
        double queuedAt{};

        bool Matches(std::uint32_t consumedBase, double now) const
        {
            return food && plate && base == consumedBase && now >= queuedAt &&
                   now - queuedAt <= 5.0;
        }
    };

    struct ScriptIdentity
    {
        std::uint64_t handle{};
        std::uintptr_t object{};
    };

    // 通过虚拟机句柄或脚本对象身份识别同一次动画回调。
    inline bool SameScript(ScriptIdentity ticket, ScriptIdentity callback)
    {
        return (ticket.handle && callback.handle && ticket.handle == callback.handle) ||
               (ticket.object && callback.object && ticket.object == callback.object);
    }

    inline bool
    KeepEffectTicket(bool requiresTableScript, bool hasTableScript, ScriptIdentity identity)
    {
        return (!requiresTableScript || hasTableScript) && (identity.handle || identity.object);
    }

    // 玩家离开原座位或票据超过保护时间后，允许恢复。
    inline bool ShouldRestore(bool atOriginalSeat, double age)
    {
        // A potion effect may already be detached while its latent EAS stack runs.
        // Native effect-list membership must not end an animation ticket.
        return !atOriginalSeat || age > 120.0;
    }

    // 仍有食物或仍在动画隐藏期间的餐盘需要保护。
    inline bool ProtectPlate(bool foodStillPresent, bool hiddenForAnimation)
    {
        return foodStillPresent || hiddenForAnimation;
    }
}
