#pragma once
#include "DiningAPI.h"
#include <map>
#include <optional>

namespace inn::dining {
struct Receipt {
  Use use;
  std::int32_t inventoryBefore{};
};
// Core owns one claim per reference. Sequence never resets on load/revert, so
// an old callback cannot finish a different meal after a new game/load.
class Claims {
  std::uint64_t sequence{};
  std::map<std::uint64_t, Receipt> active;

public:
  std::optional<Receipt> Begin(Use use, std::int32_t inventoryBefore) {
    for (const auto &[_, receipt] : active)
      // Inventory rollback compares the captured count of this base item.
      // Serialize nested uses of an identical base, even for two world refs.
      if (receipt.use.meal.food == use.meal.food ||
          receipt.use.meal.base == use.meal.base)
        return {};
    if (!use.meal.food || !use.meal.base || active.size() >= 64)
      return {};
    if (++sequence == 0)
      ++sequence;
    use.ticket = sequence;
    Receipt receipt{use, inventoryBefore};
    active.emplace(sequence, receipt);
    return receipt;
  }
  std::optional<Receipt> Take(std::uint64_t ticket) {
    auto it = active.find(ticket);
    if (it == active.end())
      return {};
    auto receipt = it->second;
    active.erase(it);
    return receipt;
  }
  void Clear() { active.clear(); }
};
} // namespace inn::dining
