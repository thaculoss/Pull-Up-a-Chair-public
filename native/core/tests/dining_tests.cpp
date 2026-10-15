#include "DiningClaims.h"
#include "DiningInteraction.h"
#include "InteractionRuntime.h"
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
static_assert(meal::activateSlot == 0x37);
static_assert(meal::activateTextSlot == 0x4C);
static_assert(meal::DrinkSlot(false) == 0x10F);
static_assert(meal::DrinkSlot(true) == 0x111);
int checks{}, applied{}, finished{}, rolledBack{}, inventory = 4;
inn::dining::Claims claims;
std::map<std::uint32_t, bool> available{{100, true}, {101, true}};
void Check(bool pass) {
  ++checks;
  if (!pass)
    throw std::runtime_error("Dining check " + std::to_string(checks));
}
bool Query(std::uint32_t id, inn::dining::ServingInfo *out) {
  if (!out || out->size < sizeof(*out) || !available.contains(id))
    return false;
  *out = {};
  out->reference = id;
  out->base = 200;
  out->committed = available[id];
  return true;
}
bool Begin(std::uint32_t id, inn::dining::Use *out) {
  if (!out || out->size < sizeof(*out) || !available.contains(id) ||
      !available[id])
    return false;
  inn::dining::Use use;
  use.meal = {id, 300, 200};
  use.order = 9;
  auto receipt = claims.Begin(use, inventory);
  if (!receipt)
    return false;
  ++inventory;
  *out = receipt->use;
  return true;
}
bool Finish(std::uint64_t ticket, bool success) {
  auto receipt = claims.Take(ticket);
  if (!receipt)
    return false;
  ++finished;
  if (success)
    available.erase(receipt->use.meal.food);
  else if (inventory > receipt->inventoryBefore) {
    --inventory;
    ++rolledBack;
  }
  return true;
}
bool Label(char *, std::uint32_t) { return true; }
const inn::dining::API api{
    sizeof(inn::dining::API), 1, Query, Begin, Finish, Label};
} // namespace
int main() {
  try {
    using namespace inn::dining;
    Check(Compatible(&api));
    Check(!Compatible(nullptr));
    auto bad = api;
    bad.version = 2;
    Check(!Compatible(&bad));
    bad = api;
    bad.size = 8;
    Check(!Compatible(&bad));
    bad = api;
    bad.Finish = nullptr;
    Check(!Compatible(&bad));
    Check(meal::UseManaged(nullptr, 100, [](auto &) {
            ++applied;
            return true;
          }) == meal::UseResult::refused);
    Check(applied == 0 && inventory == 4);
    available[100] = false;
    Check(meal::UseManaged(&api, 100, [](auto &) {
            ++applied;
            return true;
          }) == meal::UseResult::refused);
    Check(applied == 0 && finished == 0);
    available[100] = true;
    Use a, b;
    Check(api.Begin(100, &a));
    Check(!api.Begin(100, &b));
    Check(!api.Begin(101, &b)); // Serialize nested use of the same base count.
    Check(api.Finish(a.ticket, false));
    Check(!api.Finish(a.ticket, true));
    Check(available.contains(100));
    Check(api.Begin(
        101, &b)); // The distinct meal is usable after the first completes.
    Check(a.ticket != b.ticket && a.meal.base == b.meal.base);
    Check(api.Finish(b.ticket, false));
    Check(inventory == 4 && rolledBack == 2);
    Check(meal::UseManaged(&api, 100, [](auto &use) {
            ++applied;
            Check(use.meal.food == 100 && use.meal.plate == 300);
            Use duplicate;
            Check(!api.Begin(100, &duplicate));
            --inventory;
            return true;
          }) == meal::UseResult::succeeded);
    Check(!available.contains(100) && inventory == 4 && finished == 3);
    Check(meal::UseManaged(&api, 100, [](auto &) {
            ++applied;
            return true;
          }) == meal::UseResult::refused);
    Check(applied == 1);
    Check(meal::UseManaged(&api, 101, [](auto &) {
            ++applied;
            return false;
          }) == meal::UseResult::failed);
    Check(available.contains(101) && inventory == 4 && finished == 4);
    try {
      meal::UseManaged(&api, 101, [](auto &) -> bool {
        throw std::runtime_error("fixture");
      });
    } catch (const std::runtime_error &) {
    }
    Check(inventory == 4 && finished == 5);
    Check(api.Begin(101, &a));
    claims.Clear();
    --inventory; // Load restores inventory independently.
    Check(api.Begin(101, &b));
    Check(a.ticket != b.ticket);
    Check(!api.Finish(a.ticket, true));
    Check(api.Finish(b.ticket, false));
    Check(inventory == 4 && available.contains(101));
    std::cout << checks << " dining ownership/rollback checks passed\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
