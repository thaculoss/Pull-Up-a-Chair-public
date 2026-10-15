#pragma once
#include <array>
#include <cstdint>
#include <string_view>

namespace inn::service::scenes {
enum class StopStep { request, retryWait, shutdownWait, dispatchFailed, shutdownTimeout };
// A failed dispatch must never advance into "waiting for shutdown". The caller
// polls the engine scene independently; callback completion alone is not proof.
struct StopRequest {
    std::uint32_t scene{};
    unsigned attempts{};
    float age{}, nextAttempt{}, acceptedAt{};
    bool accepted{};
    static constexpr unsigned maxAttempts = 3;
    static constexpr float retryInterval = .5f, shutdownTimeout = 5.f;
    StopStep Poll(std::uint32_t id, float dt) {
        if (scene != id) { *this = {}; scene = id; }
        if (dt > 0.f && dt <= .5f) age += dt;
        if (accepted) return age - acceptedAt >= shutdownTimeout ? StopStep::shutdownTimeout : StopStep::shutdownWait;
        if (attempts >= maxAttempts) return StopStep::dispatchFailed;
        return age >= nextAttempt ? StopStep::request : StopStep::retryWait;
    }
    void Dispatched(bool sent) {
        ++attempts;
        accepted = sent;
        if (sent) acceptedAt = age;
        nextAttempt = age + retryInterval;
    }
};
struct ChattyScene { std::uint32_t scene, quest; std::string_view sceneName, questName; };
// Reviewed non-follower chatter only. The patch builder checks both editor IDs;
// runtime resolves the exact scene/parent pair from the plugin, with its patch
// loaded. Scene editor IDs are not reliably retained by the game engine.
inline constexpr std::array<ChattyScene, 10> chatty{{
    {0x819,0x802,"JB1ChatEngine","JB1ChattyNPCsEngine"},
    {0x822,0x821,"JB1ChatDo","JB1Chat"},
    {0x903,0x902,"JB1ChattyRudeDO","JB1ChattyRude"},
    {0x954,0x953,"JB1RaceReactionDO","JB1RaceReaction"},
    {0x9F1,0x9F0,"JB1HealthDO","JB1Health"},
    {0xA61,0xA60,"JB1BadHealthDO","JB1BadHealth"},
    {0xAC0,0xABF,"JB1GoodHealthDO","JB1GoodHealth"},
    {0xAFC,0xAFB,"JB1HealthChoiceDO","JB1HealthChoice"},
    {0xB1B,0xB1A,"JB1DiseaseDO","JB1Disease"},
    {0xC2A,0xC29,"JB1MehDoDO","JB1MehDo"},
}};
inline bool ChattyIdentity(std::uint32_t scene, std::uint32_t quest, std::string_view sceneName, std::string_view questName) {
    for (const auto& row : chatty)
        if (row.scene == scene && row.quest == quest && row.sceneName == sceneName && row.questName == questName) return true;
    return false;
}
}
