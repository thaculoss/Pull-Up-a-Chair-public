#include "ServiceLease.h"
#include "InnServiceBackend.h"
#include <cstddef>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace {
int checks{};
void Check(bool value, const char* message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}
innservice::backend::API CompleteBackend() {
    innservice::backend::API result{};
    result.service.resolve = [](std::uint32_t, innservice::api::Supply*) { return false; };
    result.service.inventory = [](std::uint32_t, innservice::api::StockItem*, std::uint32_t) { return std::uint32_t{}; };
    result.service.request = [] { return std::uint32_t{}; };
    result.service.state = [](innservice::api::Service*) { return false; };
    result.service.retain = [](std::uint32_t) { return false; };
    result.service.release = [](std::uint32_t) { return false; };
    result.service.cancel = [] {};
    result.legacyReady = [](bool) {};
    result.ready = [] { return true; };
    return result;
}
void BridgeCompatibility() {
    using namespace innservice;
    static_assert(std::is_standard_layout_v<backend::API>);
    static_assert(std::is_standard_layout_v<api::API>);
    static_assert(offsetof(backend::API, service) == 2 * sizeof(std::uint32_t));
    static_assert(sizeof(api::Service) == 32 && sizeof(api::Supply) == 28);
    static_assert(std::is_trivially_copyable_v<api::Service>);

    auto good = CompleteBackend();
    Check(backend::Compatible(&good), "Complete shared backend ABI rejected");
    Check(!backend::Compatible(nullptr), "Missing core backend accepted");
    for (int fault = 0; fault < 13; ++fault) {
        auto broken = good;
        switch (fault) {
        case 0: --broken.size; break;
        case 1: ++broken.version; break;
        case 2: --broken.service.size; break;
        case 3: ++broken.service.version; break;
        case 4: broken.service.resolve = nullptr; break;
        case 5: broken.service.inventory = nullptr; break;
        case 6: broken.service.request = nullptr; break;
        case 7: broken.service.state = nullptr; break;
        case 8: broken.service.retain = nullptr; break;
        case 9: broken.service.release = nullptr; break;
        case 10: broken.service.cancel = nullptr; break;
        case 11: broken.legacyReady = nullptr; break;
        case 12: broken.ready = nullptr; break;
        }
        Check(!backend::Compatible(&broken), "Unsafe partial or mismatched backend ABI accepted");
    }
    auto oversized = good;
    ++oversized.size;
    Check(!backend::Compatible(&oversized), "Unknown larger private ABI accepted without an explicit version contract");
    good.ready = [] { return false; };
    Check(backend::Compatible(&good) && !good.ready(), "Temporary load barrier was confused with ABI compatibility");

    const auto BackendRoute=innservice::backend::SelectRoute;
    using innservice::backend::Route;
    Check(BackendRoute(false, false) == Route::standalone, "Service-only installation cannot operate alone");
    Check(BackendRoute(false, true) == Route::standalone, "Stale compatibility state selected an absent core");
    Check(BackendRoute(true, true) == Route::core, "Compatible core did not own service");
    Check(BackendRoute(true, false) == Route::unavailable, "Incompatible loaded core silently created a second service owner");
}
void SessionOwnership() {
    using inn::service::Lease;
    Lease lease;
    Check(!lease.Retain(0) && !lease.Release(0) && !lease.MarkReady(), "Idle lease accepted callbacks");
    const auto old = lease.Begin();
    Check(old && old <= Lease::maxToken, "Begin returned a null or negative Papyrus token");
    Check(lease.Retain(old) && lease.MarkReady(), "Live service could not retain or notify readiness");
    Check(!lease.MarkReady(), "Repeated arrival sent the ready event twice");
    lease.Advance(20);
    const auto current = lease.Begin();
    Check(current != old && !lease.holds && !lease.readySent && lease.heldSeconds == 0,
          "New request inherited old listeners or the old timeout");
    Check(!lease.Retain(old) && !lease.Release(old), "Late callback from an old service changed the current lease");
    Check(!lease.Release(current), "Unbalanced release underflowed the hold count");
    Check(lease.Retain(current) && lease.MarkReady(), "New request could not emit its own readiness event");
    lease.Invalidate(); // A game load discards callbacks from the previous world.
    Check(!lease.token && !lease.holds && !lease.readySent && lease.heldSeconds == 0,
          "Loading preserved a live service lease");
    Check(!lease.Release(current) && !lease.Retain(current) && !lease.MarkReady(),
          "Callback survived load invalidation");
    const auto loaded = lease.Begin();
    Check(loaded != current && lease.Retain(loaded), "First request after load reused the discarded token");
    const auto sequence = lease.sequence;
    lease.End(); // Successful order commit must release the service bridge.
    Check(!lease.token && !lease.holds && !lease.readySent && lease.heldSeconds == 0 && lease.sequence == sequence,
          "Commit did not end service or reset sequence identity");
    Check(!lease.Release(loaded) && !lease.Retain(loaded), "Late order callback revived a committed lease");
    Check(lease.Begin() != loaded, "Next order reused the committed token");
}
void BoundedRetention() {
    using inn::service::Lease;
    Lease lease;
    const auto token = lease.Begin();
    for (unsigned i = 0; i < Lease::maxHolds; ++i)
        Check(lease.Retain(token), "Valid consumer exceeded retention capacity too early");
    Check(!lease.Retain(token) && lease.holds == 16, "Unbounded callbacks retained service indefinitely");
    Check(lease.Release(token) && lease.Retain(token) && lease.holds == 16, "Released capacity was not reusable");
    Check(!lease.Advance(179) && lease.holds == 16, "Service hold expired before 180 seconds");
    Check(lease.Release(token) && lease.Retain(token), "Renewal setup failed");
    Check(lease.Advance(1) && !lease.holds && lease.heldSeconds == 180,
          "Repeated Retain renewed the total service deadline");
    Check(!lease.Retain(token) && !lease.Release(token) && !lease.Advance(1),
          "Expired holders could rearm or emit repeated expiration");
    const auto fresh = lease.Begin();
    Check(lease.Retain(fresh), "Expired service poisoned the next request");
    Check(!lease.Advance(100) && lease.Release(fresh), "First timed hold failed");
    Check(!lease.Advance(1000) && lease.heldSeconds == 100, "Time without a consumer consumed its hold budget");
    Check(lease.Retain(fresh) && lease.Advance(80), "Release/retain cycling reset the cumulative deadline");

    const auto clean = lease.Begin();
    lease.Retain(clean);
    Check(!lease.Advance(-10) && !lease.Advance(0) &&
          !lease.Advance(std::numeric_limits<float>::quiet_NaN()) &&
          !lease.Advance(std::numeric_limits<float>::infinity()) && lease.heldSeconds == 0 && lease.holds == 1,
          "Invalid elapsed time corrupted the service hold");
    Check(lease.Advance(std::numeric_limits<float>::max()) && lease.heldSeconds == 180,
          "Large frame interval overflowed the bounded timeout");
}
void TokenWrap() {
    using inn::service::Lease;
    Lease lease{Lease::maxToken - 1};
    Check(lease.Begin() == Lease::maxToken, "Maximum positive Papyrus token was skipped");
    Check(lease.Begin() == 1, "Token wrap entered zero or signed-negative Papyrus range");
    Lease load{Lease::maxToken};
    load.Invalidate();
    Check(load.sequence == 1 && !load.token && load.Begin() == 2, "Load invalidation failed to wrap token sequence");
    Lease zero{0}, large{std::numeric_limits<std::uint32_t>::max()};
    Check(zero.sequence && zero.sequence <= Lease::maxToken && large.sequence && large.sequence <= Lease::maxToken,
          "Invalid seed escaped the signed-positive token domain");
}
}
int main() {
    try {
        BridgeCompatibility();
        SessionOwnership();
        BoundedRetention();
        TokenWrap();
        std::cout << checks << " service backend and lease checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
