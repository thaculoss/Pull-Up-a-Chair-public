#pragma once
#include <cstdint>

namespace inn {
// Exact identities of our removed dynamic servings. An engine-created REFR can
// retain its initial save data after its live deleted/disabled flags disappear.
// Keep this separate from paid food: recovery must never serve or charge it.
struct RetiredServing {
  std::uint32_t reference{}, base{}, cell{};
};
static_assert(sizeof(RetiredServing) == 12);
inline constexpr std::uint32_t retiredServingRecord = 0x494F5452; // IOTR
inline bool ValidRetirement(const RetiredServing &row) {
  return (row.reference >> 24) == 0xFF && row.base && row.cell;
}
inline bool MatchesRetirement(const RetiredServing &row,
                              std::uint32_t reference, std::uint32_t base,
                              std::uint32_t cell) {
  return ValidRetirement(row) && row.reference == reference &&
         row.base == base && row.cell == cell;
}
} // namespace inn
