#pragma once
#include "DiningAPI.h"
#include <utility>

namespace meal {
enum class UseResult { refused, failed, succeeded };
// Original EquipObject/DrinkPotion runs inside apply. Core receives exactly one
// terminal result, including a C++ exception; its receipt owns rollback.
template <class Apply>
UseResult UseManaged(const inn::dining::API *api, std::uint32_t reference,
                     Apply &&apply) {
  if (!inn::dining::Compatible(api))
    return UseResult::refused;
  inn::dining::Use use;
  if (!api->Begin(reference, &use))
    return UseResult::refused;
  struct Completion {
    const inn::dining::API *api;
    std::uint64_t ticket;
    bool done{};
    ~Completion() {
      if (!done)
        api->Finish(ticket, false);
    }
  } completion{api, use.ticket};
  const bool success = std::forward<Apply>(apply)(use);
  completion.done = true;
  const bool accepted = api->Finish(use.ticket, success);
  return success && accepted ? UseResult::succeeded : UseResult::failed;
}
} // namespace meal
