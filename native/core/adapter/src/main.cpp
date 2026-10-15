// 中文阅读说明
// 可选 EAS 原生适配器：匹配桌上食物、餐盘、食用效果和动画脚本的生命周期。
// 阅读重点：用精确的引用与脚本身份识别动画；隐藏、恢复餐盘和释放动画票据分别处理。

#include "DiningInteraction.h"
#include "ExtensionAPI.h"
#include "InteractionRuntime.h"
#include "NativeObserver.h"
#include "Policy.h"
#include "RuntimePolicy.h"
#include <MinHook.h>
#include <deque>
#undef GetObject

namespace {
using Clock = std::chrono::steady_clock;

double Now() {
  return std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
}

struct Pair {
  RE::FormID food{}, plate{};
};

static_assert(sizeof(Pair) == 8);

struct Visual {
  RE::NiPointer<RE::NiAVObject> root;
  RE::COL_LAYER layer{};
  bool culled{};
};

struct ObserverState {
  std::atomic_bool done{};
  std::string value;
};

struct BridgeTicket : meal::Ticket {
  meal::ScriptIdentity script;
  RE::BSTSmartPointer<RE::BSScript::Object> owner;
  RE::ObjectRefHandle seat;
  RE::FormID cell{};
  std::shared_ptr<ObserverState> state;
  bool startQueued{}, finishQueued{}, stateFailed{};
};

struct PendingAnimation {
  meal::Origin origin;
  RE::FormID spell{};
  bool confirmed{};
  std::uint64_t sequence{};
  std::vector<meal::EffectKey> before;
};

std::recursive_mutex gate;
std::unordered_map<RE::FormID, Pair> pairs;
std::unordered_set<RE::FormID> localFoods;
RE::FormID sceneCell{};
bool sceneDirty{true};
bool verboseLogging{};
std::vector<BridgeTicket> tickets;
std::unordered_map<RE::FormID, Visual> visuals;
meal::Origin pendingOrigin;
thread_local meal::Origin activationOrigin;
PendingAnimation pendingAnimation;
std::uint64_t consumptionSequence{};
std::atomic_bool ready{}, watching{}, queued{};
std::unordered_set<RE::FormID> orderPlates;
bool bridgeEnabled = true;
bool easEnabled{};
const inn::dining::API *diningAPI{};
struct ConsumptionCapture {
  RE::Actor *actor{};
  RE::FormID base{};
  bool succeeded{};
};
thread_local ConsumptionCapture *consumptionCapture{};
std::atomic_uint64_t epoch{};
std::jthread worker;
bool observerReady{};
std::atomic_bool signalTaskQueued{};

struct ObservedSignal {
  easobserver::Signal value;
  double queuedAt{};
};

std::deque<ObservedSignal> observedSignals;
void FlushObservedSignals();
void PollObservedState();
void RequestSignalTask();

bool ManagedInfo(RE::TESObjectREFR *ref, inn::dining::ServingInfo &info) {
  // Never acquire the core ledger while holding this module's mutex.
  return diningAPI && ref && diningAPI->Query(ref->GetFormID(), &info);
}
void CaptureConsumption(RE::Actor *actor, RE::AlchemyItem *food, bool success) {
  if (consumptionCapture && actor == consumptionCapture->actor && food &&
      food->GetFormID() == consumptionCapture->base)
    consumptionCapture->succeeded = success;
}
void EatManaged(RE::TESObjectREFR *ref, RE::PlayerCharacter *player) {
  try {
    const auto result = meal::UseManaged(
        diningAPI, ref->GetFormID(), [player](const inn::dining::Use &use) {
          auto food = RE::TESForm::LookupByID<RE::AlchemyItem>(use.meal.base);
          auto equip = RE::ActorEquipManager::GetSingleton();
          if (!player || !food || !equip)
            return false;
          ConsumptionCapture capture{player, use.meal.base, false};
          struct Context {
            ConsumptionCapture *previous;
            meal::Origin origin;
            ~Context() {
              consumptionCapture = previous;
              activationOrigin = origin;
            }
          } context{consumptionCapture, activationOrigin};
          consumptionCapture = &capture;
          activationOrigin = {use.meal.food, use.meal.plate, use.meal.base,
                              Now()};
          equip->EquipObject(player, food, nullptr, 1, nullptr, false, false,
                             true, true);
          return capture.succeeded;
        });
    logger::info("Dining module use ref={:08X} result={} EAS={}",
                 ref->GetFormID(), static_cast<int>(result), easEnabled);
  } catch (const std::exception &e) {
    logger::error("Dining module use rolled back: {}", e.what());
  }
}

RE::TESObjectREFR *Ref(RE::FormID id) {
  return RE::TESForm::LookupByID<RE::TESObjectREFR>(id);
}

bool Gone(RE::TESObjectREFR *r) {
  return !r || r->IsDisabled() || r->IsDeleted() || r->IsMarkedForDeletion();
}

bool OurPlate(RE::TESObjectREFR *r) {
  return r && r->GetBaseObject() && r->GetBaseObject()->GetFormID() == 0xB9BD0;
}

meal::EffectKey Key(RE::ActiveEffect *e) {
  return e ? meal::EffectKey{reinterpret_cast<std::uintptr_t>(e),
                             e->spell ? e->spell->GetFormID() : 0,
                             e->usUniqueID}
           : meal::EffectKey{};
}

std::vector<meal::EffectKey> Effects(RE::Actor *actor) {
  std::vector<meal::EffectKey> out;
  // Multi-runtime CommonLib headers do not have the game's concrete Actor
  // base layout. An implicit MagicTarget cast used +0x78 here, while SE
  // 1.5.97 requires +0x98; use the runtime-aware base accessor.
  if (actor)
    if (auto list = actor->AsMagicTarget()->GetActiveEffectList())
      for (auto e : *list)
        if (e && !e->flags.any(RE::ActiveEffect::Flag::kDispelled))
          out.push_back(Key(e));
  return out;
}

bool CaptureTicket(RE::Actor *actor, meal::EffectKey key,
                   const meal::Origin &origin,
                   bool requireTableScript = false) {
  if (std::any_of(tickets.begin(), tickets.end(),
                  [&](auto &t) { return t.effect == key; }))
    return true;
  BridgeTicket ticket;
  ticket.effect = key;
  ticket.plate = origin.plate;
  ticket.created = Now();
  ticket.seat = actor->GetOccupiedFurniture();
  if (auto cell = actor->GetParentCell())
    ticket.cell = cell->GetFormID();
  auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
  auto policy = vm ? vm->GetObjectHandlePolicy() : nullptr;
  if (policy) {
    auto handle =
        policy->GetHandleForObject(RE::ActiveEffect::VMTYPEID,
                                   reinterpret_cast<const void *>(key.address));
    if (handle != policy->EmptyHandle()) {
      ticket.script.handle = handle;
      for (auto name : {"aaaEAS_Onehands_table", "aaaEAS_Twohands_table"})
        if (vm->FindBoundObject(handle, name, ticket.owner) && ticket.owner)
          break;
      ticket.script.object =
          reinterpret_cast<std::uintptr_t>(ticket.owner.get());
    }
  }
  // Food buffs without an EAS table script cannot participate in the bridge.
  // Explicit EAS spells may be observed before the script is bound; preserve
  // their valid VM handle so the existing Begin/End matching still works.
  if (!meal::KeepEffectTicket(requireTableScript,
                              static_cast<bool>(ticket.owner), ticket.script))
    return false;
  logger::debug("Captured effect spell={:08X} handle={:X} script={:X} "
                "plate={:08X} cell={:08X} seat={}",
                key.spell, ticket.script.handle, ticket.script.object,
                ticket.plate, ticket.cell, static_cast<bool>(ticket.seat));
  tickets.push_back(std::move(ticket));
  return true;
}

// 对照激活前后的效果列表，识别这份桌上食物新触发的动画。
void ObserveAnimation(RE::Actor *actor) {
  if (!pendingAnimation.origin.food)
    return;
  if (Now() - pendingAnimation.origin.queuedAt > 120) {
    logger::info("Pending EAS spell expired for main {:08X}",
                 pendingAnimation.origin.food);
    pendingAnimation = {};
    return;
  }
  if (!pendingAnimation.confirmed || !actor)
    return;
  bool found = false;
  for (auto key : Effects(actor)) {
    // Gourmet/SkyPatcher food effects bind their Papyrus table script after
    // DrinkPotion returns. Retry the exact food's new effects as well as the
    // explicit spell selected by EAS. A nutrition buff must never qualify.
    auto route = meal::ClassifyAnimationEffect(
        key, pendingAnimation.origin.base, pendingAnimation.spell,
        pendingAnimation.before);
    if (route == meal::AnimationRoute::none)
      continue;
    if (CaptureTicket(actor, key, pendingAnimation.origin,
                      route == meal::AnimationRoute::directFood)) {
      logger::info(
          "Linked {} EAS effect {:08X} to world main {:08X}, plate {:08X}",
          route == meal::AnimationRoute::directFood ? "direct-food"
                                                    : "selected-spell",
          key.spell, pendingAnimation.origin.food,
          pendingAnimation.origin.plate);
      found = true;
    }
  }
  if (found) {
    pendingAnimation = {};
  }
}

// 记录 EAS 实际选择的动画法术，帮助后续效果匹配。
void ObserveSelectedSpell(RE::TESForm *food, RE::SpellItem *animation) {
  if (!food || !animation)
    return;
  std::scoped_lock lock(gate);
  if (!ready || !pendingAnimation.origin.food ||
      pendingAnimation.origin.base != food->GetFormID()) {
    logger::debug("EAS selected spell {:08X} for food {:08X}; no matching "
                  "world-meal request",
                  animation->GetFormID(), food->GetFormID());
    return;
  }
  pendingAnimation.spell = animation->GetFormID();
  watching = true;
  logger::debug(
      "EAS selected actual animation spell {:08X} for world main {:08X}",
      pendingAnimation.spell, pendingAnimation.origin.food);
}

bool Locked(RE::FormID plate) {
  return std::any_of(pairs.begin(), pairs.end(),
                     [plate](auto &kv) { return kv.second.plate == plate; });
}

bool Hidden(RE::FormID plate) {
  return meal::HiddenForAnimation(plate, tickets);
}

bool Protected(RE::FormID plate) {
  return orderPlates.contains(plate) ||
         meal::ProtectPlate(Locked(plate), Hidden(plate));
}

// 按保存状态恢复动画期间暂时隐藏的餐盘。
void Restore(Visual &v) {
  if (v.root) {
    v.root->SetAppCulled(v.culled);
    v.root->SetCollisionLayer(v.layer);
  }
}

void Apply(RE::FormID id) {
  const bool protect = Protected(id);
  auto r = Ref(id);
  auto root = OurPlate(r) && !Gone(r) ? r->Get3D() : nullptr;
  auto it = visuals.find(id);
  if (it != visuals.end() && (!protect || it->second.root.get() != root)) {
    Restore(it->second);
    visuals.erase(it);
  }
  if (!protect || !root)
    return;
  auto [v, fresh] = visuals.try_emplace(id);
  if (fresh)
    v->second = {RE::NiPointer<RE::NiAVObject>(root), root->GetCollisionLayer(),
                 root->GetAppCulled()};
  // Transient scene state only. Never persist a disabled / activation-blocked
  // plate in the save. The misc activation-text hook also filters BTPS scans.
  if (root->GetCollisionLayer() != RE::COL_LAYER::kNonCollidable)
    root->SetCollisionLayer(RE::COL_LAYER::kNonCollidable);
  const bool culled = v->second.culled || Hidden(id);
  if (root->GetAppCulled() != culled)
    root->SetAppCulled(culled);
}

void UpdateWatching() {
  watching = !orderPlates.empty() || !pairs.empty() || !tickets.empty() ||
             !visuals.empty() || pendingOrigin.food ||
             pendingAnimation.origin.food || !observedSignals.empty();
}

void RebuildScene(RE::Actor *player) {
  auto cell = player->GetParentCell();
  auto nextCell = cell ? cell->GetFormID() : 0;
  auto belongsHere = [cell](const meal::Origin &origin) {
    auto plate = Ref(origin.plate);
    return cell && plate && plate->GetParentCell() == cell;
  };
  // A fresh order may already have started in the new cell before this tick.
  // Cancel only a pending request whose actual paired plate is elsewhere.
  if (pendingOrigin.food && !belongsHere(pendingOrigin))
    pendingOrigin = {};
  if (pendingAnimation.origin.food && !belongsHere(pendingAnimation.origin))
    pendingAnimation = {};
  sceneCell = nextCell;
  localFoods.clear();
  for (auto it = pairs.begin(); it != pairs.end();) {
    auto food = Ref(it->first);
    auto plate = Ref(it->second.plate);
    if (cell && food && plate && food->GetParentCell() == cell &&
        plate->GetParentCell() == cell) {
      localFoods.insert(it->first);
      ++it;
    } else {
      // User preference: leaving the inn ends plate protection. The
      // actual food/plate references are neither moved nor deleted.
      logger::debug("Stopped tracking departed meal {:08X}", it->first);
      it = pairs.erase(it);
    }
  }
  // Do not retain an old cell's 3D graph just to protect its distant plate.
  for (auto it = visuals.begin(); it != visuals.end();) {
    auto plate = Ref(it->first);
    if (!plate || !cell || plate->GetParentCell() != cell) {
      Restore(it->second);
      it = visuals.erase(it);
    } else
      ++it;
  }
  sceneDirty = false;
  logger::debug("Scene refreshed: local meals={} stored pairs={}",
                localFoods.size(), pairs.size());
}

void Refresh() {
  std::unordered_set<RE::FormID> ids;
  for (auto id : orderPlates)
    ids.insert(id);
  for (auto food : localFoods)
    if (auto it = pairs.find(food); it != pairs.end())
      ids.insert(it->second.plate);
  for (auto &t : tickets)
    ids.insert(t.plate);
  for (auto &[id, _] : visuals)
    ids.insert(id);
  for (auto id : ids)
    Apply(id);
  UpdateWatching();
}

void Release(RE::FormID food) {
  auto it = pairs.find(food);
  if (it == pairs.end())
    return;
  auto plate = it->second.plate;
  pairs.erase(it);
  localFoods.erase(food);
  logger::info("Main removed {:08X}; plate {:08X} orderManaged={} protected={}",
               food, plate, orderPlates.contains(plate), Protected(plate));
  Apply(plate);
  UpdateWatching();
}

// 观察动画、座位及有效引用，更新隐藏与恢复状态。
void Tick() {
  std::scoped_lock lock(gate);
  if (!ready || !RE::PlayerCharacter::GetSingleton() ||
      RE::UI::GetSingleton()->GameIsPaused())
    return;
  auto player = RE::PlayerCharacter::GetSingleton();
  auto currentCell = player->GetParentCell();
  std::erase_if(orderPlates, [&](auto id) {
    auto ref = Ref(id);
    return Gone(ref) || !currentCell || ref->GetParentCell() != currentCell;
  });
  if (easEnabled &&
      (sceneDirty || (currentCell ? currentCell->GetFormID() : 0) != sceneCell))
    RebuildScene(player);
  if (easEnabled) {
    FlushObservedSignals();
    ObserveAnimation(player);
    PollObservedState();
  }
  if (pendingOrigin.food && Now() - pendingOrigin.queuedAt > 5.0)
    pendingOrigin = {};
  const auto seat = player->GetOccupiedFurniture();
  const auto cell = player->GetParentCell();
  for (auto it = localFoods.begin(); it != localFoods.end();) {
    auto pair = pairs.find(*it);
    if (pair == pairs.end()) {
      it = localFoods.erase(it);
      continue;
    }
    if (Gone(Ref(pair->second.food)) || Gone(Ref(pair->second.plate))) {
      pairs.erase(pair);
      it = localFoods.erase(it);
    } else
      ++it;
  }
  std::erase_if(tickets, [&](const auto &t) {
    bool atSeat = cell && cell->GetFormID() == t.cell && seat && seat == t.seat;
    bool expired = meal::ShouldRestore(atSeat, Now() - t.created);
    if (expired)
      logger::info(
          "Ticket cleared plate={:08X} hidden={} atOriginalSeat={} age={:.2f}",
          t.plate, t.hidden, atSeat, Now() - t.created);
    return expired;
  });
  Refresh();
}

template <class F> void Queue(F fn) {
  auto generation = epoch.load();
  SKSE::GetTaskInterface()->AddTask([generation, fn = std::move(fn)]() {
    std::scoped_lock lock(gate);
    if (ready && generation == epoch.load())
      fn();
  });
}

// 登记一份世界食物及它的餐盘配对。
void RegisterPair(RE::StaticFunctionTag *, RE::TESObjectREFR *food,
                  RE::TESObjectREFR *plate) {
  if (!food || !plate || !OurPlate(plate) || !food->GetBaseObject() ||
      !food->GetBaseObject()->As<RE::AlchemyItem>())
    return;
  auto foodHandle = food->GetHandle(), plateHandle = plate->GetHandle();
  Queue([foodHandle, plateHandle]() {
    auto f = foodHandle.get(), p = plateHandle.get();
    if (Gone(f.get()) || Gone(p.get()) || !OurPlate(p.get()))
      return;
    auto id = f->GetFormID();
    pairs[id] = {id, p->GetFormID()};
    localFoods.insert(id);
    sceneDirty = true;
    Apply(p->GetFormID());
    watching = true;
    logger::info("Paired main {:08X} -> plate {:08X}", id, p->GetFormID());
  });
}

// 用脚本调用身份找到同一次动画的开始或结束票据。
void BridgeCall(bool begin, RE::Actor *actor,
                std::vector<meal::ScriptIdentity> identities) {
  if (!actor || !actor->IsPlayerRef()) {
    logger::debug("EAS {} skipped: non-player or missing actor",
                  begin ? "Begin" : "End");
    return;
  }
  if (identities.empty()) {
    logger::warn("EAS {} skipped: no script identity in argument or caller",
                 begin ? "Begin" : "End");
    return;
  }
  logger::info("EAS {} received: handle={:X} script={:X} candidates={}",
               begin ? "Begin" : "End", identities[0].handle,
               identities[0].object, identities.size());
  Queue([begin, identities = std::move(identities)]() {
    auto player = RE::PlayerCharacter::GetSingleton();
    if (!player) {
      logger::warn("EAS bridge skipped: no player");
      return;
    }
    ObserveAnimation(player);
    bool matched = false;
    for (auto it = tickets.begin(); it != tickets.end();) {
      auto &t = *it;
      bool same =
          std::any_of(identities.begin(), identities.end(),
                      [&](auto id) { return meal::SameScript(t.script, id); });
      if (!same) {
        ++it;
        continue;
      }
      matched = true;
      if (begin) {
        if (t.finishQueued) {
          ++it;
          continue;
        }
        auto cell = player->GetParentCell();
        auto seat = player->GetOccupiedFurniture();
        if (!cell || cell->GetFormID() != t.cell || !seat || seat != t.seat) {
          logger::warn(
              "EAS Begin refused: seat or cell changed for plate {:08X}",
              t.plate);
          it = tickets.erase(it);
          continue;
        }
        t.hidden = true;
        Apply(t.plate);
        auto plate = Ref(t.plate);
        auto node = plate ? plate->Get3D() : nullptr;
        logger::info("EAS tableware begin; hide plate {:08X}; root={} "
                     "culled={} age={:.2f}",
                     t.plate, node != nullptr, node && node->GetAppCulled(),
                     Now() - t.created);
        ++it;
      } else {
        logger::info("EAS finish; release animation visibility plate {:08X}; "
                     "wasHidden={} orderManaged={}",
                     t.plate, t.hidden, orderPlates.contains(t.plate));
        it = tickets.erase(it);
      }
    }
    if (!matched)
      logger::info(
          "EAS {} ignored: no matching exact world-meal script; tickets={}",
          begin ? "Begin" : "End", tickets.size());
    Refresh();
  });
}

bool IsTableObject(const easobserver::Object &object) {
  auto type = object ? object->GetTypeInfo() : nullptr;
  return type && (_stricmp(type->GetName(), "aaaEAS_Onehands_table") == 0 ||
                  _stricmp(type->GetName(), "aaaEAS_Twohands_table") == 0);
}

const easobserver::Frame *FindCaller(const easobserver::Frame &frame,
                                     const char *type,
                                     const char *function = nullptr) {
  auto caller = &frame;
  for (unsigned i = 0; caller && i < 12; ++i, caller = caller->previousFrame)
    if (caller->owningObjectType &&
        _stricmp(caller->owningObjectType->GetName(), type) == 0 &&
        (!function ||
         (caller->owningFunction &&
          _stricmp(caller->owningFunction->GetName().c_str(), function) == 0)))
      return caller;
  return nullptr;
}

std::optional<easobserver::Signal>
CaptureObservedCall(easobserver::Kind kind, easobserver::Variable &base,
                    const easobserver::Frame &frame, RE::VMStackID stack) {
  if (!ready || !observerReady)
    return {};
  easobserver::Signal result;
  result.kind = kind;
  result.epoch = epoch.load();
  result.stack = stack;
  const auto page = frame.GetPageForFrame();
  auto ref = frame.GetStackFrameVariable(0, page).Unpack<RE::TESObjectREFR *>();
  if (!ref || !ref->IsPlayerRef())
    return {};
  result.actor = ref->GetFormID();
  if (kind == easobserver::Kind::remote) {
    auto caller =
        FindCaller(frame, "aaaEASQuestAliasScript", "OnObjectEquipped");
    if (!caller)
      return {};
    auto food = caller->GetStackFrameVariable(0, caller->GetPageForFrame())
                    .Unpack<RE::TESForm *>();
    auto spell = base.Unpack<RE::SpellItem *>();
    if (!food || !spell)
      return {};
    std::scoped_lock lock(gate);
    if (!pendingAnimation.origin.food ||
        pendingAnimation.origin.base != food->GetFormID())
      return {};
    result.sequence = pendingAnimation.sequence;
    result.food = food->GetFormID();
    result.spell = spell->GetFormID();
  } else if (kind == easobserver::Kind::animation) {
    auto &command = frame.GetStackFrameVariable(1, page);
    if (!command.IsString() || !meal::TablewareStart(command.GetString()))
      return {};
    auto caller = FindCaller(frame, "aaaEAS_Onehands_table");
    if (!caller)
      caller = FindCaller(frame, "aaaEAS_Twohands_table");
    if (!caller || !caller->self.IsObject())
      return {};
    result.owner = caller->self.GetObject();
    if (!IsTableObject(result.owner))
      return {};
    result.handle = result.owner->GetHandle();
    if (auto vm = easobserver::VM::GetSingleton())
      if (auto policy = vm->GetObjectHandlePolicy();
          policy && result.handle == policy->EmptyHandle())
        result.handle = 0;
    result.command = command.GetString();
  } else
    return {};
  return result;
}

void PublishObservedCall(easobserver::Signal signal) {
  {
    std::scoped_lock lock(gate);
    if (!ready || signal.epoch != epoch.load() || !signal.dispatched)
      return;
    if (observedSignals.size() >= 128) {
      logger::error("Observer signal queue full; skipped without selecting "
                    "another plate");
      return;
    }
    observedSignals.push_back({std::move(signal), Now()});
    watching = true;
  }
  RequestSignalTask();
}

void RequestSignalTask() {
  if (!signalTaskQueued.exchange(true)) {
    const auto generation = epoch.load();
    SKSE::GetTaskInterface()->AddTask([generation]() {
      std::scoped_lock lock(gate);
      if (ready && generation == epoch.load())
        FlushObservedSignals();
      signalTaskQueued = false;
    });
  }
}

void FlushObservedSignals() {
  auto player = RE::PlayerCharacter::GetSingleton();
  if (!player)
    return;
  // Capture the actual selected spell first, even if several VM calls reached
  // the game task together. Never use a legacy TrackAnimation notification.
  for (auto it = observedSignals.begin(); it != observedSignals.end();) {
    const auto &signal = it->value;
    if (signal.epoch != epoch.load()) {
      it = observedSignals.erase(it);
      continue;
    }
    if (signal.kind != easobserver::Kind::remote) {
      ++it;
      continue;
    }
    if (meal::CurrentSpellObservation(epoch.load(), pendingAnimation.sequence,
                                      pendingAnimation.origin.base,
                                      signal.epoch, signal.sequence,
                                      signal.food)) {
      ObserveSelectedSpell(
          RE::TESForm::LookupByID(signal.food),
          RE::TESForm::LookupByID<RE::SpellItem>(signal.spell));
      logger::info(
          "Observer selected spell {:08X} for world main {:08X} sequence={}",
          signal.spell, pendingAnimation.origin.food, signal.sequence);
    } else
      logger::debug("Stale observer spell ignored sequence={}",
                    signal.sequence);
    it = observedSignals.erase(it);
  }
  ObserveAnimation(player);
  for (auto it = observedSignals.begin(); it != observedSignals.end();) {
    auto &signal = it->value;
    BridgeTicket *match{};
    unsigned matches{};
    const meal::ScriptIdentity identity{
        signal.handle, reinterpret_cast<std::uintptr_t>(signal.owner.get())};
    for (auto &ticket : tickets)
      if (meal::SameScript(ticket.script, identity)) {
        match = &ticket;
        ++matches;
      }
    if (matches == 1) {
      if (!match->owner) {
        match->owner = signal.owner;
        match->script.object = identity.object;
      }
      if (!match->startQueued && !match->finishQueued) {
        match->startQueued = true;
        logger::info("Observer EU start command={} script={:X} plate={:08X}",
                     signal.command, identity.object, match->plate);
        BridgeCall(true, player, {match->script});
      }
      it = observedSignals.erase(it);
    } else if (matches > 1 || Now() - it->queuedAt > 5) {
      logger::info("Observer EU start ignored: exact matches={} script={:X}",
                   matches, identity.object);
      it = observedSignals.erase(it);
    } else
      ++it; // Binding may complete after RemoteCast/DrinkPotion returns.
  }
}

struct StateCompletion final : RE::BSScript::IStackCallbackFunctor {
  explicit StateCompletion(std::shared_ptr<ObserverState> out)
      : result(std::move(out)) {}

  std::shared_ptr<ObserverState> result;

  void operator()(RE::BSScript::Variable value) override {
    result->value =
        value.IsString() ? std::string(value.GetString()) : "<invalid>";
    result->done.store(true, std::memory_order_release);
  }

  void SetObject(const easobserver::Object &) override {}
};

void PollObservedState() {
  auto vm = easobserver::VM::GetSingleton();
  auto player = RE::PlayerCharacter::GetSingleton();
  if (!vm || !player)
    return;
  for (auto &ticket : tickets) {
    if (ticket.finishQueued || ticket.stateFailed)
      continue;
    if (ticket.state && ticket.state->done.load(std::memory_order_acquire)) {
      const auto value = ticket.state->value;
      ticket.state.reset();
      if (_stricmp(value.c_str(), "busy") == 0) {
        ticket.finishQueued = true;
        logger::info(
            "Observer script finished; state={} script={:X} plate={:08X}",
            value, ticket.script.object, ticket.plate);
        BridgeCall(false, player, {ticket.script});
        continue;
      }
      if (value == "<invalid>") {
        ticket.stateFailed = true;
        logger::warn("Observer state result unavailable for plate {:08X}; "
                     "seat/timeout recovery remains active",
                     ticket.plate);
        continue;
      }
    }
    if (!ticket.owner && ticket.script.handle)
      for (auto name : {"aaaEAS_Onehands_table", "aaaEAS_Twohands_table"})
        if (vm->FindBoundObject(ticket.script.handle, name, ticket.owner) &&
            ticket.owner) {
          ticket.script.object =
              reinterpret_cast<std::uintptr_t>(ticket.owner.get());
          break;
        }
    if (ticket.owner && !ticket.state) {
      ticket.state = std::make_shared<ObserverState>();
      const auto generation = epoch.load();
      // Do not enter the VM dispatcher while holding the plate mutex.
      // The callback owns only its result; a load/scene reset can safely
      // discard the ticket before an outstanding query completes.
      SKSE::GetTaskInterface()->AddTask(
          [owner = ticket.owner, out = ticket.state, generation]() mutable {
            if (!ready || generation != epoch.load())
              return;
            auto targetVM = easobserver::VM::GetSingleton();
            RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{
                new StateCompletion(out)};
            if (!targetVM ||
                !targetVM->DispatchMethodCall(
                    owner, "GetState", RE::MakeFunctionArguments(), callback)) {
              out->value = "<invalid>";
              out->done.store(true, std::memory_order_release);
            }
          });
    }
  }
}

// Compatibility bindings for old saves/leftover PEX. The override-free adapter
// does not read injected Begin/End arguments or resolve dead native effects.
using BridgeSignature = void(RE::StaticFunctionTag *, RE::Actor *,
                             RE::ActiveEffect *);

class BridgeFunction final : public RE::NativeFunction<BridgeSignature> {
public:
  explicit BridgeFunction(const char *name)
      : RE::NativeFunction<BridgeSignature>(
            name, "PullUpAChairEAS",
            +[](RE::StaticFunctionTag *, RE::Actor *, RE::ActiveEffect *) {}) {}

  bool
  MarshallAndDispatch(RE::BSScript::Variable &,
                      RE::BSScript::Internal::VirtualMachine &vm, RE::VMStackID,
                      RE::BSScript::Variable &result,
                      const RE::BSScript::StackFrame &frame) const override {
    // Old saves/leftover third-party PEX can still call this API. Observer
    // mode never consumes injected notifications or resolves a dead effect.
    (void)vm;
    (void)frame;
    result.SetNone();
    return true;
  }
};

// 注册 EAS 脚本使用的桥接函数。
bool RegisterPapyrus(RE::BSScript::IVirtualMachine *vm) {
  vm->RegisterFunction("RegisterPair", "PullUpAChairEAS", RegisterPair);
  vm->RegisterFunction(
      "TrackAnimation", "PullUpAChairEAS",
      +[](RE::StaticFunctionTag *, RE::TESForm *, RE::SpellItem *) {});
  vm->RegisterFunction(
      "IsDiagnosticEnabled", "PullUpAChairEAS",
      +[](RE::StaticFunctionTag *) { return verboseLogging; });
  return vm->BindNativeMethod(new BridgeFunction("Begin")) &&
         vm->BindNativeMethod(new BridgeFunction("End"));
}

struct FoodActivate {
  static bool thunk(RE::TESBoundObject *base, RE::TESObjectREFR *target,
                    RE::TESObjectREFR *activator, std::uint8_t arg3,
                    RE::TESBoundObject *object, std::int32_t count) {
    meal::Origin origin{};
    auto player = RE::PlayerCharacter::GetSingleton();
    if (ready && target && activator == player) {
      inn::dining::ServingInfo info;
      if (ManagedInfo(target, info)) {
        if (!info.committed)
          return false;
        if (info.kind != 1) {
          EatManaged(target, player);
          return true;
        }
      }
    }
    {
      std::scoped_lock lock(gate);
      if (ready && target && activator == player) {
        auto it = pairs.find(target->GetFormID());
        if (it != pairs.end())
          origin = {it->second.food, it->second.plate, base->GetFormID(),
                    Now()};
      }
    }
    // Chain through the finished Use Or Take / shop plugin hook, preserving
    // its take/eat binding and return value. No synthetic equip or activation.
    const auto outer = activationOrigin;
    activationOrigin = origin;
    const bool result = next(base, target, activator, arg3, object, count);
    activationOrigin = outer;
    if (origin.food) {
      std::scoped_lock lock(gate);
      if (Gone(Ref(origin.food)))
        Release(origin.food);
      logger::info("World main activated {:08X}", origin.food);
    }
    return result;
  }

  static inline REL::Relocation<decltype(thunk)> next;
};

struct FoodText {
  static bool thunk(RE::TESBoundObject *base, RE::TESObjectREFR *ref,
                    RE::BSString &text) {
    inn::dining::ServingInfo info;
    if (ready && ManagedInfo(ref, info)) {
      if (!info.committed) {
        text = "";
        return false;
      }
      char label[128]{};
      if (diningAPI->CopyEatLabel(label, sizeof(label))) {
        text = fmt::format("{}\n{}", label, base->GetName());
        return true;
      }
    }
    return next(base, ref, text);
  }
  static inline REL::Relocation<decltype(thunk)> next;
};

// EquipObject can enqueue consumption. Capture the explicit equip requested by
// Use Or Take while still in the exact food reference's activation. Any
// separate equip request cancels the pending origin; do not infer origin from a
// hotkey or merely seeing the same base item disappear from the player's
// inventory.
struct EquipObject {
  static void thunk(RE::ActorEquipManager *self, RE::Actor *actor,
                    RE::TESBoundObject *object, RE::ExtraDataList *extra,
                    std::uint32_t count, const RE::BGSEquipSlot *slot,
                    bool queue, bool force, bool sounds, bool applyNow) {
    if (actor && actor->IsPlayerRef()) {
      std::scoped_lock lock(gate);
      pendingOrigin = {};
      if (ready && object && activationOrigin.food &&
          activationOrigin.base == object->GetFormID()) {
        pendingOrigin = activationOrigin;
        pendingOrigin.queuedAt = Now();
        watching = true;
        logger::info("World meal consumption requested {:08X}, queue={}",
                     pendingOrigin.food, queue);
      }
    }
    next(self, actor, object, extra, count, slot, queue, force, sounds,
         applyNow);
  }

  static inline decltype(&thunk) next{};

  static void Install() {
    auto status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) {
      logger::error("Origin hook unavailable: {}", MH_StatusToString(status));
      return;
    }
    REL::Relocation<std::uintptr_t> equip{RELOCATION_ID(37938, 38894)};
    void *target = reinterpret_cast<void *>(equip.address());
    status = MH_CreateHook(target, reinterpret_cast<void *>(thunk),
                           reinterpret_cast<void **>(&next));
    if (status == MH_OK)
      status = MH_EnableHook(target);
    logger::info("Equip origin hook: {}", MH_StatusToString(status));
  }
};

struct DrinkPotion {
  static bool thunk(RE::Actor *actor, RE::AlchemyItem *potion,
                    RE::ExtraDataList *extra) {
    if (!easEnabled) {
      const bool success = next(actor, potion, extra);
      CaptureConsumption(actor, potion, success);
      return success;
    }
    meal::Origin origin{};
    std::vector<meal::EffectKey> before;
    const auto generation = epoch.load();
    std::uint64_t sequence{};
    {
      std::scoped_lock lock(gate);
      if (ready && actor && actor->IsPlayerRef() && potion &&
          pendingOrigin.Matches(potion->GetFormID(), Now())) {
        origin = pendingOrigin;
        before = Effects(actor);
      }
      pendingOrigin = {};
      if (actor && actor->IsPlayerRef()) {
        sequence = ++consumptionSequence;
        pendingAnimation = {};
        if (origin.food) {
          pendingAnimation.origin = origin;
          pendingAnimation.origin.queuedAt = Now();
          pendingAnimation.sequence = sequence;
          pendingAnimation.before = before;
        }
      }
    }
    const bool success = next(actor, potion, extra);
    CaptureConsumption(actor, potion, success);
    {
      std::scoped_lock lock(gate);
      if (pendingAnimation.sequence == sequence &&
          pendingAnimation.origin.food) {
        if (success) {
          pendingAnimation.confirmed = true;
          watching = true;
        } else
          pendingAnimation = {};
      }
    }
    if (success && origin.food) {
      std::scoped_lock lock(gate);
      if (ready && epoch.load() == generation) {
        unsigned issued = 0;
        for (auto key : Effects(actor))
          if (meal::IsNewMealEffect(key, origin.base, before)) {
            if (CaptureTicket(actor, key, origin, true))
              ++issued;
          }
        Release(origin.food);
        Refresh();
        logger::info("World meal consumed {:08X}; new effect tickets={}",
                     origin.food, issued);
      }
    }
    return success;
  }

  static inline REL::Relocation<decltype(thunk)> next;
};

struct PlateText {
  static bool thunk(RE::TESBoundObject *base, RE::TESObjectREFR *ref,
                    RE::BSString &text) {
    inn::dining::ServingInfo info;
    if (ready && ManagedInfo(ref, info) && info.kind == 1) {
      text = "";
      return false;
    }
    {
      std::scoped_lock lock(gate);
      if (ready && ref && Protected(ref->GetFormID())) {
        text = "";
        return false;
      }
    }
    return next(base, ref, text);
  }

  static inline REL::Relocation<decltype(thunk)> next;
};

struct PlateActivate {
  static bool thunk(RE::TESBoundObject *base, RE::TESObjectREFR *target,
                    RE::TESObjectREFR *activator, std::uint8_t arg3,
                    RE::TESBoundObject *object, std::int32_t count) {
    inn::dining::ServingInfo info;
    if (ready && ManagedInfo(target, info) && info.kind == 1)
      return false;
    {
      std::scoped_lock lock(gate);
      if (ready && target && Protected(target->GetFormID()))
        return false;
    }
    return next(base, target, activator, arg3, object, count);
  }

  static inline REL::Relocation<decltype(thunk)> next;
};

class ContainerSink final
    : public RE::BSTEventSink<RE::TESContainerChangedEvent> {
  RE::BSEventNotifyControl
  ProcessEvent(const RE::TESContainerChangedEvent *event,
               RE::BSTEventSource<RE::TESContainerChangedEvent> *) override {
    if (event && event->newContainer)
      if (auto ref = event->reference.get()) {
        std::scoped_lock lock(gate);
        Release(ref->GetFormID());
      }
    return RE::BSEventNotifyControl::kContinue;
  }
} containerSink;

void Clear() {
  std::scoped_lock lock(gate);
  ++epoch;
  watching = false;
  for (auto &[_, visual] : visuals)
    Restore(visual);
  visuals.clear();
  pairs.clear();
  orderPlates.clear();
  tickets.clear();
  observedSignals.clear();
  pendingOrigin = {};
  pendingAnimation = {};
  localFoods.clear();
  sceneCell = 0;
  sceneDirty = true;
}

constexpr std::uint32_t kID = 0x4C444D50; // LDMP
constexpr std::uint32_t kPair = 0x50414952;

void Save(SKSE::SerializationInterface *io) {
  std::scoped_lock lock(gate);
  for (auto &[_, p] : pairs)
    if (!Gone(Ref(p.food)) && !Gone(Ref(p.plate)))
      io->WriteRecord(kPair, 1, p);
}

void Load(SKSE::SerializationInterface *io) {
  Clear();
  std::scoped_lock lock(gate);
  std::uint32_t type, version, length;
  while (io->GetNextRecordInfo(type, version, length)) {
    if (type != kPair || version != 1 || length != sizeof(Pair))
      continue;
    Pair saved{}, resolved{};
    if (io->ReadRecordData(saved) == sizeof(Pair) &&
        io->ResolveFormID(saved.food, resolved.food) &&
        io->ResolveFormID(saved.plate, resolved.plate))
      pairs[resolved.food] = resolved;
  }
  watching = !pairs.empty();
  logger::info("Loaded {} main/plate pairs; animation visibility reset",
               pairs.size());
}

void CoreMessage(SKSE::MessagingInterface::Message *message) {
  if (!message || message->dataLen != sizeof(inn::extension::Meal) ||
      !message->data)
    return;
  const auto &value = *static_cast<const inn::extension::Meal *>(message->data);
  static thread_local std::vector<meal::Origin> origins;
  if (message->type == inn::extension::orderPlate) {
    std::scoped_lock lock(gate);
    orderPlates.insert(value.plate);
    UpdateWatching();
  } else if (message->type == inn::extension::registerMeal && easEnabled)
    RegisterPair(nullptr, Ref(value.food), Ref(value.plate));
  else if (message->type == inn::extension::consumeBegin) {
    origins.push_back(activationOrigin);
    activationOrigin = {value.food, value.plate, value.base, Now()};
  } else if (message->type == inn::extension::consumeEnd && !origins.empty()) {
    activationOrigin = origins.back();
    origins.pop_back();
  }
}

void Message(SKSE::MessagingInterface::Message *message) {
  if (!bridgeEnabled)
    return;
  switch (message->type) {
  case SKSE::MessagingInterface::kPostLoad:
    SKSE::GetMessagingInterface()->RegisterListener("PullUpAChairOrdering",
                                                    CoreMessage);
    break;
  case SKSE::MessagingInterface::kDataLoaded: {
    auto module = GetModuleHandleW(L"PullUpAChairOrdering.dll");
    auto getAPI = module ? reinterpret_cast<inn::dining::GetAPI>(
                               GetProcAddress(module, inn::dining::factory))
                         : nullptr;
    diningAPI = getAPI ? getAPI(inn::dining::version) : nullptr;
    if (!inn::dining::Compatible(diningAPI))
      diningAPI = nullptr;
    auto data = RE::TESDataHandler::GetSingleton();
    const auto runtime = REL::Module::get().version();
    const bool observerRuntime = runtime == REL::Version{1, 5, 97, 0} ||
                                 runtime == REL::Version{1, 7, 104, 0};
    easEnabled = data && data->LookupLoadedModByName("TaberuAnimation.esp") &&
                 GetPrivateProfileIntW(
                     L"Compatibility", L"EAS", 1,
                     L".\\Data\\SKSE\\Plugins\\PullUpAChairEAS.ini") != 0 &&
                 observerRuntime;
    logger::info("Dining module1 API={} native interactions={} EAS={}; core "
                 "owns receipts and saves",
                 diningAPI != nullptr, bridgeEnabled, easEnabled);
    if (!diningAPI)
      logger::warn("Dining API unavailable; managed click-to-eat remains with "
                   "legacy core if present");
    bool remote = false, animation = false;
    if (easEnabled) {
      easobserver::NativeObserver::Configure(CaptureObservedCall,
                                             PublishObservedCall);
      remote = easobserver::NativeObserver::Install<0>(
          "Spell", "RemoteCast", easobserver::Kind::remote, false, 3);
      animation = easobserver::NativeObserver::Install<1>(
          "Debug", "SendAnimationEvent", easobserver::Kind::animation, true, 2);
      observerReady = remote && animation;
    }
    logger::info("EAS observer hooks ready={} remote={} animation={}; no EAS "
                 "PEX overrides or injected notifications required",
                 observerReady, remote, animation);
    REL::Relocation<std::uintptr_t> food{RE::VTABLE_AlchemyItem[0]};
    REL::Relocation<std::uintptr_t> misc{RE::VTABLE_TESObjectMISC[0]};
    REL::Relocation<std::uintptr_t> player{RE::VTABLE_PlayerCharacter[0]};
    if (easEnabled)
      EquipObject::Install();
    FoodActivate::next =
        food.write_vfunc(meal::activateSlot, FoodActivate::thunk);
    FoodText::next = food.write_vfunc(meal::activateTextSlot, FoodText::thunk);
    DrinkPotion::next = player.write_vfunc(
        meal::DrinkSlot(REL::Module::IsVR()), DrinkPotion::thunk);
    PlateText::next =
        misc.write_vfunc(meal::activateTextSlot, PlateText::thunk);
    PlateActivate::next =
        misc.write_vfunc(meal::activateSlot, PlateActivate::thunk);
    logger::info("Hooks installed; chained food Activate={:X}; misc text={:X}",
                 FoodActivate::next.address(), PlateText::next.address());
    RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink(&containerSink);
    ready = true;
    worker = std::jthread([](std::stop_token stop) {
      while (!stop.stop_requested()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        if (ready && watching && !queued.exchange(true)) {
          auto generation = epoch.load();
          SKSE::GetTaskInterface()->AddTask([generation]() {
            if (ready && generation == epoch.load())
              Tick();
            queued = false;
          });
        }
      }
    });
    break;
  }
  case SKSE::MessagingInterface::kPreLoadGame:
    ready = false;
    Clear();
    break;
  case SKSE::MessagingInterface::kPostLoadGame:
    ready = true;
    Tick();
    break;
  case SKSE::MessagingInterface::kNewGame:
    Clear();
    ready = true;
    break;
  default:
    break;
  }
}
} // namespace

SKSEPluginLoad(const SKSE::LoadInterface *skse) {
  auto path = SKSE::log::log_directory();
  if (!path)
    return false;
  *path /= "PullUpAChairEAS.log";
  auto log = std::make_shared<spdlog::logger>(
      "PullUpAChairEAS", std::make_shared<spdlog::sinks::basic_file_sink_mt>(
                             path->string(), true));
  verboseLogging = GetPrivateProfileIntW(
                       L"Diagnostics", L"VerboseLogging", 0,
                       L".\\Data\\SKSE\\Plugins\\PullUpAChairEAS.ini") != 0;
  bridgeEnabled = GetPrivateProfileIntW(
                      L"General", L"Enabled", 1,
                      L".\\Data\\SKSE\\Plugins\\PullUpAChairEAS.ini") != 0;
  spdlog::set_default_logger(log);
  spdlog::set_pattern("[%T.%e] [%=5t] [%L] %v");
  spdlog::set_level(verboseLogging ? spdlog::level::debug
                                   : spdlog::level::info);
  spdlog::flush_on(verboseLogging ? spdlog::level::debug : spdlog::level::warn);
  spdlog::flush_every(std::chrono::seconds(3));
  logger::info("Pull Up a Chair diningmodule1 (no script overrides), runtime "
               "{}; diagnostics={}; optional native food/plate interactions "
               "and EAS observation",
               skse->RuntimeVersion().string(), verboseLogging);
  logger::info("Optional EAS bridge enabled={}; disabled mode retains Papyrus "
               "bindings without installing hooks",
               bridgeEnabled);
  const auto version = skse->RuntimeVersion();
  if (!inn::Supported(
          {version.major(), version.minor(), version.patch(), version.build()}))
    return false;
  logger::info("Runtime mode={}; gameplay validation required",
               inn::ExperimentalVR({version.major(), version.minor(),
                                    version.patch(), version.build()})
                   ? "experimental VR (EAS animation observation unavailable)"
                   : "flat runtime");
  // CommonLib's default Init recreates/truncates the logger and resets its
  // level/flush policy. Keep the diagnostic settings configured above.
  SKSE::Init(skse, {.log = false});
  if (!SKSE::GetPapyrusInterface()->Register(RegisterPapyrus))
    return false;
  auto io = SKSE::GetSerializationInterface();
  io->SetUniqueID(kID);
  io->SetSaveCallback(Save);
  io->SetLoadCallback(Load);
  io->SetRevertCallback([](SKSE::SerializationInterface *) { Clear(); });
  return SKSE::GetMessagingInterface()->RegisterListener(Message);
}
