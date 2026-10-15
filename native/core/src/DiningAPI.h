#pragma once
#include "ExtensionAPI.h"
#include <cstdint>

// Borrowed process-lifetime API. Query/CopyEatLabel are read-only; Begin/Finish
// run on the game thread. No STL objects, engine pointers or locks cross DLLs.
namespace inn::dining {
inline constexpr std::uint32_t version = 1;
inline constexpr auto factory = "PullUpAChairOrdering_GetDiningAPI";
struct ServingInfo {
  std::uint32_t size{sizeof(ServingInfo)}, reference{}, base{}, plate{},
      order{}, kind{}, committed{};
};
struct Use {
  std::uint32_t size{sizeof(Use)}, reserved{};
  std::uint64_t ticket{};
  extension::Meal meal{};
  std::uint32_t order{};
};
struct API {
  std::uint32_t size{}, version{};
  bool (*Query)(std::uint32_t reference, ServingInfo *out);
  bool (*Begin)(std::uint32_t reference, Use *out);
  bool (*Finish)(std::uint64_t ticket, bool succeeded);
  bool (*CopyEatLabel)(char *destination, std::uint32_t capacity);
};
using GetAPI = const API *(*)(std::uint32_t requested);
inline bool Compatible(const API *api) {
  return api && api->size >= sizeof(API) && api->version == version &&
         api->Query && api->Begin && api->Finish && api->CopyEatLabel;
}
static_assert(sizeof(ServingInfo) == 28 && sizeof(Use) == 32 &&
              sizeof(API) == 40);
} // namespace inn::dining
