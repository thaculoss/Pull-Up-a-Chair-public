#include "TableManager.h"
#include "DailyMenus.h"
#include "DiningClaims.h"
#include "ExtensionAPI.h"
#include "InnServices.h"
#include "Localization.h"
#include "PCH.h"
#include "ServiceApproach.h"
#include "ServingRetirement.h"
#include "SupplierServices.h"
#include "SurfacePolicy.h"
#include "TableCleanup.h"
#include "TablePolicy.h"
#include <set>
#include <unordered_set>
namespace inn::tables {
namespace {
std::recursive_mutex mutex;
std::map<RE::FormID, Serving> servings;
std::map<RE::FormID, RetiredServing> retired;
dining::Claims claims;
std::atomic_uint64_t notificationEpoch{1};
bool recovering = false;
std::uint32_t orderSequence = 0;
constexpr std::uint32_t recordType = 0x494F5442;
RE::TESObjectREFR *Ref(RE::FormID id) {
  return RE::TESForm::LookupByID<RE::TESObjectREFR>(id);
}
bool Exists(RE::TESObjectREFR *ref) {
  return ref && !ref->IsDeleted() && !ref->IsMarkedForDeletion();
}
bool Matches(RE::TESObjectREFR *ref, const RetiredServing &row) {
  return ref && ref->GetBaseObject() && ref->GetParentCell() &&
         MatchesRetirement(row, ref->GetFormID(),
                           ref->GetBaseObject()->GetFormID(),
                           ref->GetParentCell()->GetFormID());
}
void Remove(RE::TESObjectREFR *ref) {
  if (Exists(ref)) {
    if (auto found = servings.find(ref->GetFormID()); found != servings.end()) {
      const auto &row = found->second;
      retired[row.reference] = {row.reference, row.base, row.cell};
    }
    ref->Disable();
    ref->SetDelete(true);
    // SetDelete may remove the previous change entry while Papyrus/another
    // plugin still holds a created reference. Persist its terminal form flags
    // as well as our exact-identity receipt, instead of leaving initial data.
    ref->AddChange(RE::TESForm::ChangeFlags::kFlags);
  }
}
void RecoverRetired(RE::FormID cell = 0) {
  unsigned removed = 0;
  for (auto i = retired.begin(); i != retired.end();) {
    if (cell && i->second.cell != cell) {
      ++i;
      continue;
    }
    auto ref = Ref(i->first);
    // The cell may not be attached yet. Keep its receipt until the matching
    // cell event; do not search the room or infer ownership from a food base.
    if (!ref || !ref->GetBaseObject() || !ref->GetParentCell()) {
      ++i;
    } else if (Matches(ref, i->second)) {
      if (Exists(ref)) {
        Remove(ref);
        ++removed;
      }
      ++i;
    } else {
      i = retired.erase(i);
    }
  }
  if (!cell || removed)
    logger::info("mealrestore1 retired serving recovery removed={} retained={}; "
                 "exact reference/base/cell only", removed, retired.size());
}
void Dispatch(std::uint32_t type, extension::Meal &meal) {
  // Notifications never call an adapter while the core ledger mutex is held.
  // A load/revert invalidates queued messages from the previous world.
  const auto epoch = notificationEpoch.load();
  SKSE::GetTaskInterface()->AddTask([type, meal, epoch]() mutable {
    if (notificationEpoch.load() == epoch)
      SKSE::GetMessagingInterface()->Dispatch(type, &meal, sizeof(meal),
                                              nullptr);
  });
}
void UpdatePlate(std::uint32_t order, bool forceRemove = false) {
  int remaining = 0;
  for (const auto &[id, row] : servings)
    if (row.order == order && row.committed && row.kind != 1 && Exists(Ref(id)))
      ++remaining;
  auto player = RE::PlayerCharacter::GetSingleton();
  std::vector<RE::FormID> plates;
  for (const auto &[id, row] : servings)
    if (row.order == order && row.committed && row.kind == 1)
      plates.push_back(id);
  for (const auto id : plates)
    if (auto ref = Ref(id); Exists(ref)) {
      const auto row = servings.at(id);
      extension::Meal scope{0, id, 0};
      Dispatch(extension::orderPlate, scope);
      const bool sameSeat =
          player && player->GetOccupiedFurniture().get() &&
          player->GetOccupiedFurniture().get()->GetFormID() == row.seat &&
          IsSeated(player);
      if (forceRemove || RemovePlate(sameSeat, remaining)) {
        Remove(ref);
        servings.erase(id);
        logger::info(
            "Order plate removed ref={:08X} order={} remaining={} seated={}",
            id, order, remaining, sameSeat);
      }
    }
}
class ContainerEvents final
    : public RE::BSTEventSink<RE::TESContainerChangedEvent> {
  RE::BSEventNotifyControl
  ProcessEvent(const RE::TESContainerChangedEvent *event,
               RE::BSTEventSource<RE::TESContainerChangedEvent> *) override {
    if (event && event->newContainer)
      if (auto ref = event->reference.get()) {
        std::lock_guard lock(mutex);
        if (auto i = servings.find(ref->GetFormID()); i != servings.end()) {
          const auto order = i->second.order;
          servings.erase(i);
          UpdatePlate(order);
        }
      }
    return RE::BSEventNotifyControl::kContinue;
  }
} containerEvents;
class FurnitureEvents final : public RE::BSTEventSink<RE::TESFurnitureEvent> {
  RE::BSEventNotifyControl
  ProcessEvent(const RE::TESFurnitureEvent *event,
               RE::BSTEventSource<RE::TESFurnitureEvent> *) override {
    if (event && event->actor.get() == RE::PlayerCharacter::GetSingleton() &&
        event->type.any(RE::TESFurnitureEvent::FurnitureEventType::kExit)) {
      const auto seat =
          event->targetFurniture ? event->targetFurniture->GetFormID() : 0;
      SKSE::GetTaskInterface()->AddTask([seat] {
        std::lock_guard lock(mutex);
        std::set<std::uint32_t> orders;
        for (const auto &[id, row] : servings)
          if (row.committed && row.kind == 1 && (!seat || row.seat == seat))
            orders.insert(row.order);
        for (const auto order : orders)
          UpdatePlate(order, true);
      });
    }
    return RE::BSEventNotifyControl::kContinue;
  }
} furnitureEvents;
class CellEvents final : public RE::BSTEventSink<RE::TESCellFullyLoadedEvent> {
  RE::BSEventNotifyControl
  ProcessEvent(const RE::TESCellFullyLoadedEvent *event,
               RE::BSTEventSource<RE::TESCellFullyLoadedEvent> *) override {
    if (event && event->cell) {
      const auto cell = event->cell->GetFormID();
      const auto epoch = notificationEpoch.load();
      SKSE::GetTaskInterface()->AddTask([cell, epoch] {
        std::lock_guard lock(mutex);
        if (notificationEpoch.load() == epoch)
          RecoverRetired(cell);
      });
    }
    return RE::BSEventNotifyControl::kContinue;
  }
} cellEvents;

} // namespace
bool Track(int token, RE::TESObjectREFR *ref, RE::TESObjectREFR *seat,
           RE::TESObjectREFR *table, RE::BGSLocation *location, int kind) {
  if (!ref || !seat || !table || !location || !ref->GetParentCell() ||
      !ref->GetBaseObject() || kind < 0 || kind > 2)
    return false;
  if ((ref->GetFormID() >> 24) != 0xFF)
    return false; // Never adopt placed world/quest objects.
  std::lock_guard lock(mutex);
  if (servings.size() >= 4096)
    std::erase_if(servings,
                  [](const auto &entry) { return !Exists(Ref(entry.first)); });
  if (servings.size() >= 4096 && !servings.contains(ref->GetFormID()))
    return false;
  auto p = ref->GetPosition();
  // A new order explicitly owns this current reference, even if the engine
  // recycled an ID previously used by a retired serving.
  retired.erase(ref->GetFormID());
  servings[ref->GetFormID()] = {ref->GetFormID(),
                                ref->GetBaseObject()->GetFormID(),
                                ref->GetParentCell()->GetFormID(),
                                seat->GetFormID(),
                                table->GetFormID(),
                                location->GetFormID(),
                                static_cast<std::uint32_t>(token),
                                static_cast<std::uint32_t>(kind),
                                0,
                                p.x,
                                p.y,
                                p.z};
  return true;
}
bool Prepared(int token, const std::vector<RE::FormID> &foods, bool plate) {
  std::lock_guard lock(mutex);
  std::multiset<RE::FormID> actual, expected(foods.begin(), foods.end());
  int plates = 0;
  for (auto &[id, row] : servings)
    if (row.order == static_cast<std::uint32_t>(token) && !row.committed) {
      auto ref = Ref(id);
      if (!Exists(ref) || ref->IsDisabled() || !ref->Is3DLoaded() ||
          !ref->GetBaseObject() ||
          ref->GetBaseObject()->GetFormID() != row.base ||
          !ref->GetParentCell() ||
          ref->GetParentCell()->GetFormID() != row.cell) {
        logger::warn(
            "Prepared serving rejected order={} ref={:08X} "
            "expected_base={:08X} current_base={:08X} exists={} disabled={} "
            "loaded={} expected_cell={:08X} current_cell={:08X}",
            token, id, row.base,
            ref && ref->GetBaseObject() ? ref->GetBaseObject()->GetFormID() : 0,
            Exists(ref), ref && ref->IsDisabled(), ref && ref->Is3DLoaded(),
            row.cell,
            ref && ref->GetParentCell() ? ref->GetParentCell()->GetFormID()
                                        : 0);
        return false;
      }
      if (row.kind != 1)
        actual.insert(row.base);
      else
        ++plates;
    }
  const bool ready =
      actual == expected && plates == (plate ? 1 : 0) && !expected.empty();
  if (!ready) {
    logger::warn(
        "Prepared serving mismatch order={} food_count={}/{} plates={}/{}",
        token, actual.size(), expected.size(), plates, plate ? 1 : 0);
    for (auto base : expected)
      logger::warn("Prepared expected food order={} base={:08X} "
                   "actual_count={} expected_count={}",
                   token, base, actual.count(base), expected.count(base));
  }
  return ready;
}
void Commit(int token) {
  std::lock_guard lock(mutex);
  extension::Meal meal{};
  for (auto &[id, row] : servings)
    if (row.order == static_cast<std::uint32_t>(token) && !row.committed) {
      row.committed = 1;
      if (auto ref = Ref(id))
        ref->SetActivationBlocked(false);
      if (row.kind == 1)
        meal.plate = id;
      else if (row.kind == 0) {
        meal.food = id;
        meal.base = row.base;
      }
    }
  UpdatePlate(static_cast<std::uint32_t>(token));
  if (meal.food && meal.plate)
    Dispatch(extension::registerMeal, meal);
}
void Cancel(int token) {
  std::lock_guard lock(mutex);
  for (auto i = servings.begin(); i != servings.end();) {
    if (i->second.order == static_cast<std::uint32_t>(token) &&
        !i->second.committed) {
      Remove(Ref(i->first));
      i = servings.erase(i);
    } else
      ++i;
  }
}
void RequestClean(RE::Actor *server) { cleanup::Request(server); }
void RestoreTable(RE::Actor *server) { cleanup::Request(server, true); }
bool Managed(RE::FormID id) {
  std::lock_guard lock(mutex);
  return servings.contains(id);
}
bool HasCommittedForTable(RE::FormID seat, RE::FormID cell,
                          const std::vector<RE::FormID> &tables) {
  std::lock_guard lock(mutex);
  for (const auto &[id, row] : servings)
    if (PaidAtTable(row, seat, cell, tables) && Exists(Ref(id)))
      return true;
  return false;
}
void Discard(RE::FormID id) {
  std::lock_guard lock(mutex);
  Remove(Ref(id));
  servings.erase(id);
}
void Save(SKSE::SerializationInterface *io) {
  std::lock_guard lock(mutex);
  for (const auto &[id, row] : servings)
    if (Exists(Ref(id)))
      io->WriteRecord(recordType, 1, row);
  for (auto i = retired.begin(); i != retired.end();) {
    auto ref = Ref(i->first);
    if (!ref || !ref->GetBaseObject() || !ref->GetParentCell() ||
        Matches(ref, i->second)) {
      io->WriteRecord(retiredServingRecord, 1, i->second);
      ++i;
    } else {
      // A positively different identity means this ID has been recycled.
      i = retired.erase(i);
    }
  }
}
int NextOrder() {
  std::lock_guard lock(mutex);
  return static_cast<int>(++orderSequence);
}
int RecoveredStatus(int token) {
  std::lock_guard lock(mutex);
  for (const auto &[id, row] : servings)
    if (row.order == static_cast<std::uint32_t>(token) && row.committed)
      return 3;
  return 0;
}
void Revert() {
  std::lock_guard lock(mutex);
  servings.clear();
  retired.clear();
  claims.Clear();
  ++notificationEpoch;
  recovering = false;
  cleanup::Revert();
}
void Load(SKSE::SerializationInterface *io) {
  Revert();
  daily::Revert();
  suppliers::Revert();
  service::PrepareLoad();
  std::lock_guard lock(mutex);
  std::uint32_t type, version, length;
  while (io->GetNextRecordInfo(type, version, length)) {
    if (type == retiredServingRecord) {
      if (version == 1 && length == sizeof(RetiredServing)) {
        RetiredServing row{};
        bool valid = io->ReadRecordData(row) == sizeof(row);
        for (auto field : {&row.reference, &row.base, &row.cell}) {
          std::uint32_t resolved{};
          if (!valid || !io->ResolveFormID(*field, resolved)) {
            valid = false;
            break;
          }
          *field = resolved;
        }
        if (valid && ValidRetirement(row))
          retired[row.reference] = row;
      }
      continue;
    }
    if (type == daily::recordType) {
      daily::Load(io, version, length);
      continue;
    }
    if (type == cleanup::recordType) {
      cleanup::Load(io, version, length);
      continue;
    }
    if (type == suppliers::recordType) {
      suppliers::Load(io, version, length);
      continue;
    }
    if (type == service::recordType) {
      service::Load(io, version, length);
      continue;
    }
    if (type != recordType || version != 1 || length != sizeof(Serving) ||
        servings.size() >= 4096)
      continue;
    Serving row{};
    if (io->ReadRecordData(row) != sizeof(row))
      continue;
    bool valid = true;
    for (auto field : {&row.reference, &row.base, &row.cell, &row.seat,
                       &row.table, &row.inn}) {
      std::uint32_t resolved{};
      if (!io->ResolveFormID(*field, resolved)) {
        valid = false;
        break;
      }
      *field = resolved;
    }
    if (valid && (row.reference >> 24) == 0xFF && row.kind < 3 &&
        row.committed < 2 && std::isfinite(row.x) && std::isfinite(row.y) &&
        std::isfinite(row.z)) {
      servings[row.reference] = row;
      orderSequence = std::max(orderSequence, row.order);
    }
  }
  recovering = true;
}
void Recover() {
  std::lock_guard lock(mutex);
  if (!recovering)
    return;
  RecoverRetired();
  for (auto i = servings.begin(); i != servings.end();) {
    if (!i->second.committed) {
      Remove(Ref(i->first));
      i = servings.erase(i);
    } else {
      if (auto ref = Ref(i->first))
        ref->SetActivationBlocked(false);
      ++i;
    }
  }
  recovering = false;
  std::set<std::uint32_t> orders;
  for (const auto &[id, row] : servings)
    if (row.committed && row.kind == 1)
      orders.insert(row.order);
  for (auto order : orders)
    UpdatePlate(order);
}

bool QueryServing(std::uint32_t reference, dining::ServingInfo *out) {
  if (!out || out->size < sizeof(*out))
    return false;
  std::lock_guard lock(mutex);
  const auto found = servings.find(reference);
  if (found == servings.end())
    return false;
  const auto row = found->second;
  *out = {};
  out->reference = reference;
  out->base = row.base;
  out->order = row.order;
  out->kind = row.kind;
  out->committed = row.committed;
  for (const auto &[id, other] : servings)
    if (other.order == row.order && other.kind == 1) {
      out->plate = id;
      break;
    }
  return true;
}
bool BeginUse(std::uint32_t reference, dining::Use *out) {
  if (!out || out->size < sizeof(*out))
    return false;
  std::lock_guard lock(mutex);
  const auto found = servings.find(reference);
  if (found == servings.end() || !found->second.committed ||
      found->second.kind == 1)
    return false;
  const auto row = found->second;
  auto ref = Ref(reference);
  auto player = RE::PlayerCharacter::GetSingleton();
  if (!Exists(ref) || !player || !IsSeated(player) || player->IsInCombat() ||
      player->GetParentCell() != ref->GetParentCell() ||
      !ref->GetBaseObject() || ref->GetBaseObject()->GetFormID() != row.base)
    return false;
  auto food = ref->GetBaseObject()->As<RE::AlchemyItem>();
  if (!food)
    return false;
  dining::Use use;
  use.meal = {reference, 0, row.base};
  use.order = row.order;
  for (const auto &[id, other] : servings)
    if (other.order == row.order && other.kind == 1) {
      use.meal.plate = id;
      break;
    }
  const auto before = player->GetItemCount(food);
  const auto receipt = claims.Begin(use, before);
  if (!receipt)
    return false;
  player->AddObjectToContainer(food, nullptr, 1, nullptr);
  if (player->GetItemCount(food) != before + 1) {
    claims.Take(receipt->use.ticket);
    if (player->GetItemCount(food) > before)
      player->RemoveItem(food, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr,
                         nullptr);
    return false;
  }
  *out = receipt->use;
  return true;
}
bool FinishUse(std::uint64_t ticket, bool succeeded) {
  std::lock_guard lock(mutex);
  const auto receipt = claims.Take(ticket);
  if (!receipt)
    return false;
  const auto &use = receipt->use;
  auto player = RE::PlayerCharacter::GetSingleton();
  auto food = RE::TESForm::LookupByID<RE::AlchemyItem>(use.meal.base);
  if (!succeeded) {
    if (player && food && player->GetItemCount(food) > receipt->inventoryBefore)
      player->RemoveItem(food, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr,
                         nullptr);
    RE::SendHUDMessage::ShowHUDMessage(T("eat_failed"));
    logger::info("Dining receipt rolled back ticket={} ref={:08X}", ticket,
                 use.meal.food);
    return true;
  }
  const auto found = servings.find(use.meal.food);
  if (found == servings.end() || found->second.order != use.order ||
      found->second.base != use.meal.base) {
    logger::warn(
        "Dining receipt rejected changed ownership ticket={} ref={:08X}",
        ticket, use.meal.food);
    return false;
  }
  Remove(Ref(use.meal.food));
  servings.erase(found);
  UpdatePlate(use.order);
  logger::info(
      "Table food consumed ref={:08X} base={:08X}; dining module receipt={}",
      use.meal.food, use.meal.base, ticket);
  return true;
}
bool CopyEatLabel(char *destination, std::uint32_t capacity) {
  if (!destination || !capacity)
    return false;
  const std::string_view text = T("eat");
  const auto count = std::min<std::size_t>(text.size(), capacity - 1);
  std::copy_n(text.data(), count, destination);
  destination[count] = 0;
  return true;
}
const dining::API *DiningAPI(std::uint32_t requested) {
  static const dining::API api{sizeof(dining::API), dining::version,
                               QueryServing,        BeginUse,
                               FinishUse,           CopyEatLabel};
  return requested == dining::version ? &api : nullptr;
}

void Install() {
  RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink(&containerEvents);
  RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink(&furnitureEvents);
  RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink(&cellEvents);
}
} // namespace inn::tables

extern "C" __declspec(dllexport) const inn::dining::API *
PullUpAChairOrdering_GetDiningAPI(std::uint32_t requested) {
  return inn::tables::DiningAPI(requested);
}
