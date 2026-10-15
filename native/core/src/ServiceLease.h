#pragma once
#include <cmath>
#include <cstdint>
#include <limits>

namespace inn::service {
// Game-thread policy; runtime owners provide synchronization and elapsed time.
// Tokens stay positive when crossing Papyrus's signed 32-bit Int boundary.
struct Lease {
    static constexpr std::uint32_t maxToken = std::numeric_limits<std::int32_t>::max();
    static constexpr unsigned maxHolds = 16;
    static constexpr float holdTimeout = 180.f;

    std::uint32_t sequence;
    std::uint32_t token{};
    unsigned holds{};
    bool readySent{};
    float heldSeconds{};

    explicit constexpr Lease(std::uint32_t seed = 1)
        : sequence(seed && seed <= maxToken ? seed : 1) {}

    std::uint32_t Begin() {
        End();
        Next();
        return token = sequence;
    }
    void Invalidate() {
        End();
        Next();
    }
    bool Retain(std::uint32_t expected) {
        if (!Matches(expected) || holds >= maxHolds || heldSeconds >= holdTimeout) return false;
        ++holds;
        return true;
    }
    bool Release(std::uint32_t expected) {
        if (!Matches(expected) || !holds) return false;
        --holds;
        return true;
    }
    bool MarkReady() {
        if (!token || readySent) return false;
        readySent = true;
        return true;
    }
    // Time is cumulative across every holder in this service session. Neither
    // Retain nor a temporary return to zero holders renews the timeout budget.
    // A timed-out token cannot be retained again until a new Begin.
    bool Advance(float dt) {
        if (!holds || !(dt > 0.f) || !std::isfinite(dt)) return false;
        if (dt >= holdTimeout - heldSeconds) {
            heldSeconds = holdTimeout;
            holds = 0;
            return true;
        }
        heldSeconds += dt;
        return false;
    }
    void End() {
        token = 0;
        holds = 0;
        readySent = false;
        heldSeconds = 0;
    }

private:
    bool Matches(std::uint32_t expected) const { return token && token == expected; }
    void Next() { sequence = sequence >= maxToken ? 1 : sequence + 1; }
};
}
