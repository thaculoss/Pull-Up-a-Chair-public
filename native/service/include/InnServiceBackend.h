#pragma once
#include "InnServiceAPI.h"
// Private, versioned bridge to the ordering engine. Public consumers keep using
// PullUpAChairService_GetAPI v1. Every function is called on the SKSE game thread.
namespace innservice::backend {
inline constexpr std::uint32_t version=1;
inline constexpr char factory[]="PullUpAChairOrdering_GetServiceBackend";
inline constexpr wchar_t module[]=L"PullUpAChairOrdering.dll";
enum class Route { standalone,core,unavailable };
constexpr Route SelectRoute(bool corePresent,bool compatible){
    return !corePresent?Route::standalone:compatible?Route::core:Route::unavailable;
}
struct API {
    std::uint32_t size{sizeof(API)},version{backend::version};
    api::API service;
    // False while loading or clearing a legacy service quest alias. The bridge
    // calls true only after the old alias script has completed EvaluatePackage.
    void (*legacyReady)(bool ready);
    bool (*ready)();
};
using GetAPI=const API* (*)(std::uint32_t version);
inline bool Compatible(const API* value) {
    return value&&value->size==sizeof(API)&&value->version==version&&
        value->service.size==sizeof(api::API)&&value->service.version==api::version&&
        value->service.resolve&&value->service.inventory&&value->service.request&&
        value->service.state&&value->service.retain&&value->service.release&&
        value->service.cancel&&value->legacyReady&&value->ready;
}
}
