// 中文阅读说明
// 离线检查：动画效果身份、脚本身份、餐盘隐藏和恢复规则。
// 阅读重点：这些检查验证数据和规则；通过检查不代表游戏内行为已验证。

#include "Policy.h"
#include <cstdlib>
#include <iostream>
unsigned checks{};

void check(bool pass)
{
    ++checks;
    if (!pass)
    {
        std::cerr << "Failed check " << checks << '\n';
        std::abort();
    }
}

// 测试程序入口：依次检查各个场景，任一检查失败就报告错误。
int main()
{
    using namespace meal;
    // Actual Vortex 1.7.104 beef trace: End retained the same object after its
    // handle changed from 2000100000014 to the VM empty-handle sentinel.
    check(SameScript({0x2000100000014, 0x1FD6A91F200}, {0xFFFF00000000, 0x1FD6A91F200}));
    check(!SameScript({0x2000100000014, 0x1FD6A91F200}, {0xFFFF00000000, 0x1FD6A91F201}));
    check(TablewareStart("EasChairEuStart"));
    check(TablewareStart("EasChairEuStStart"));
    check(!TablewareStart("EasChairStart"));
    check(!TablewareStart("EasStart"));
    check(CurrentSpellObservation(3, 6, 0x721E8, 3, 6, 0x721E8));
    check(!CurrentSpellObservation(4, 6, 0x721E8, 3, 6, 0x721E8)); // Old load.
    check(!CurrentSpellObservation(3, 7, 0x721E8, 3, 6, 0x721E8)); // Next identical meal.
    check(!CurrentSpellObservation(3, 6, 0x721E8, 3, 6, 0xF4320)); // Other food.
    check(!CurrentSpellObservation(3, 0, 0, 3, 0, 0));
    EffectKey previous{0x100, 0xAA, 1}, unrelated{0x200, 0xBB, 2}, fresh{0x300, 0xAA, 3};
    check(!IsNewMealEffect(previous, 0xAA, {previous}));
    check(!IsNewMealEffect(unrelated, 0xAA, {previous}));
    check(IsNewMealEffect(fresh, 0xAA, {previous, unrelated}));
    check(!IsNewMealEffect({}, 0, {}));
    check(IsNewMealEffect({0x100, 0xAA, 4}, 0xAA, {previous}));
    // Two visually identical meals do not share effect-instance tickets.
    check(fresh != previous);
    check(ProtectPlate(true, false));   // Food still on visible plate.
    check(!ProtectPlate(false, false)); // Taken food / finished animation.
    check(ProtectPlate(false, true));   // Direct eat during tableware animation.
    // Observed rabbit-leg regression: the EAS Begin matched the exact plate,
    // but whole-order ownership vetoed culling while two other dishes remained.
    constexpr std::uint32_t orderPlate = 0xFF001BFC, neighborPlate = 0xFF001BFD;
    std::vector<Ticket> visibility{{fresh, orderPlate, false, 10},
                                   {unrelated, neighborPlate, true, 10}};
    check(!HiddenForAnimation(orderPlate, visibility)); // Main still on the table.
    visibility[0].hidden = true;                        // Exact EAS Begin.
    check(HiddenForAnimation(orderPlate, visibility));  // Hide even for a managed order.
    check(!HiddenForAnimation(0xFF001BFE, visibility)); // Never hide a neighboring order.
    visibility.erase(visibility.begin());               // EAS End, two dishes remain.
    check(!HiddenForAnimation(orderPlate, visibility)); // Restore visibility only.
    check(ProtectPlate(true, HiddenForAnimation(orderPlate, visibility))); // Keep protection.
    visibility.clear();
    check(!HiddenForAnimation(orderPlate, visibility)); // No stale ticket after cleanup.
    check(!ShouldRestore(true, 8)); // Keep ticket even when native effect has ended.
    check(ShouldRestore(false, 8));
    check(ShouldRestore(true, 121));
    check(SameScript({100, 10}, {100, 20})); // Cast base object, same VM handle.
    check(SameScript({100, 10}, {0, 10}));   // Detached handle, same retained script.
    check(!SameScript({100, 10}, {101, 20}));
    check(!SameScript({0, 0}, {0, 0}));
    check(!SameScript({100, 0}, {0, 0}));
    Origin queued{0xFF000001, 0xFF000002, 0xAA, 10};
    check(queued.Matches(0xAA, 10.5));
    // 配对窗口包含起点和第 5 秒；窗口两侧都应拒绝，防止边界误差丢失有效动画。
    check(queued.Matches(0xAA, 10));
    check(queued.Matches(0xAA, 15));
    check(!queued.Matches(0xAA, 9.99));
    check(!queued.Matches(0xAA, 15.01));
    check(!queued.Matches(0xBB, 10.5)); // A drink cannot consume a main-dish ticket.
    check(!queued.Matches(0xAA, 16));   // Failed / stale equip request expires.
    queued = {};                        // Any independent inventory equip cancels it.
    check(!queued.Matches(0xAA, 10.5));
    EffectKey animation{0x400, 0xEEE, 5};
    check(
        !IsNewMealEffect(animation, 0xAA, {previous})); // Animation is a different spell than food.
    check(IsNewMealEffect(animation, 0xEEE, {previous})); // Match EAS's explicitly selected spell.
    check(!IsNewMealEffect(animation, 0xEEE, {animation})); // Do not capture an earlier animation.
    check(!IsNewMealEffect(unrelated, 0xEEE, {previous}));  // Unrelated food/drink is excluded.
    check(!KeepEffectTicket(true, false, {0, 0}));   // Nutrition-only effect from the observed log.
    check(!KeepEffectTicket(true, false, {100, 0})); // Non-EAS food buff even with a VM handle.
    check(KeepEffectTicket(true, true, {100, 10}));  // Direct food carrying a table script.
    check(KeepEffectTicket(false, false, {100, 0})); // Explicit EAS spell, binding still pending.
    check(!KeepEffectTicket(
        false, false, {0, 0})); // Retry observation instead of retaining no identity.
    check(KeepEffectTicket(false, true, {0, 10}));
    // Regression: Gourmet's Goat Cheese Fondue has no selected EAS spell.
    // Its 10-second food effect exists before the Papyrus table script binds.
    constexpr std::uint32_t fondue = 0xF4320;
    EffectKey priorFondue{0x500, fondue, 6}, newFondue{0x600, fondue, 7};
    auto candidate =
        [&](EffectKey effect, std::uint32_t selected, bool tableBound, ScriptIdentity id)
    {
        auto route = ClassifyAnimationEffect(effect, fondue, selected, {priorFondue});
        return route != AnimationRoute::none &&
               KeepEffectTicket(route == AnimationRoute::directFood, tableBound, id);
    };
    check(ClassifyAnimationEffect(newFondue, fondue, 0, {priorFondue}) ==
          AnimationRoute::directFood);
    check(!candidate(newFondue, 0, false, {200, 0}));   // t0: script not yet bound; keep pending.
    check(candidate(newFondue, 0, true, {200, 20}));    // t1: bind observed before/at Begin.
    check(SameScript({200, 20}, {200, 30}));            // Begin can supply a cast object.
    check(SameScript({200, 20}, {0, 20}));              // End can follow native effect destruction.
    check(!candidate(priorFondue, 0, true, {100, 10})); // Earlier serving cannot be claimed.
    check(!candidate(newFondue, 0, false, {201, 21}));  // Nutrition script is not a table script.
    check(!candidate(unrelated, 0, true, {202, 22}));   // A drink/another food is not this main.
    check(!candidate({}, 0, true, {203, 23}));
    check(!candidate(newFondue, 0, true, {})); // Missing identity is retried, not accepted.
    check(candidate(
        newFondue, 0xEEE, true, {200, 20})); // Direct route also works if a spell was selected.
    check(candidate(
        animation, 0xEEE, false, {300, 0})); // Preserve deferred binding of explicit EAS spells.
    check(!candidate(animation, 0, false, {300, 0})); // Unselected spells cannot claim the plate.
    check(ClassifyAnimationEffect(animation, 0, 0, {}) == AnimationRoute::none);
    std::cout
        << checks
        << " script identity / delayed food binding / EAS capture / lifecycle checks passed\n";
}
