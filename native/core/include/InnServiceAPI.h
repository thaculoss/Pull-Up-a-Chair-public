#pragma once
#include <cstdint>
// Stable C ABI. World queries and retain/release must run on the SKSE game thread.
// This interface resolves services and existing stock; it never creates or charges orders.
namespace innservice::api {
inline constexpr std::uint32_t version=1;
enum class Status:std::uint32_t { unavailable=0, ready=1, stockOnly=2, ambiguousStock=3 };
enum class Phase:std::uint32_t { idle=0, starting=1, approaching=2, waiting=3, conversation=4, releasing=5 };
struct Supply {
    std::uint32_t size{sizeof(Supply)},inn{},merchant{},chest{},faction{},staffCount{},status{};
};
static_assert(sizeof(Supply)==28);
struct Service {
    std::uint32_t size{sizeof(Service)},token{},phase{},server{},seat{},inn{},merchant{},chest{};
};
static_assert(sizeof(Service)==32);
struct StockItem {std::uint32_t form{},count{};};
struct API {
    std::uint32_t size{sizeof(API)},version{api::version};
    bool (*resolve)(std::uint32_t server,Supply* out);
    std::uint32_t (*inventory)(std::uint32_t server,StockItem* out,std::uint32_t capacity);
    std::uint32_t (*request)();
    bool (*state)(Service* out);
    bool (*retain)(std::uint32_t token);
    bool (*release)(std::uint32_t token);
    void (*cancel)();
};
using GetAPI=const API* (*)(std::uint32_t version);
inline constexpr char factory[]="PullUpAChairService_GetAPI";
inline constexpr std::uint32_t serviceReady=0x494E5301;
// UI-only notification: uint32_t payload, 0 English or 1 Chinese. Service ABI v1 is unchanged.
inline constexpr std::uint32_t languageChanged=0x494E5303;
inline constexpr std::uint32_t serviceEnded=0x494E5302;
// SKSE messages contain a by-value Service. Copy it during the listener callback.
}
