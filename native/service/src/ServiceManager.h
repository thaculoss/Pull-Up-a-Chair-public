#pragma once
#include "InnServiceAPI.h"
#include <string>
#include <vector>
namespace innservice {
struct View {
    api::Service service;api::Supply supply;
    std::string innName,serverName,merchantName,status{"idle"};
    std::vector<std::pair<std::string,int>> stock;
};
std::uint32_t Request();
void QueueRequest(bool waitForMenu=false);
void QueueCancel();
void QueueRefresh();
void Tick();
void Recover();
void Revert();
void InitWorker();
// Co-installed ordering owns input, AI and supply; absence alone permits fallback.
bool UsesCore();
const char* BackendStatus();
void DiscoverBackend();
void QueueRecover();
void SessionLoaded();
void HandleCoreMessage(SKSE::MessagingInterface::Message* message);
bool Retain(std::uint32_t token);
bool Release(std::uint32_t token);
bool IsReleaseCurrent(int token);
void ReleaseComplete(int token);
View CurrentView();
bool CopyState(api::Service* out);
bool ResolveAPI(std::uint32_t server,api::Supply* out);
std::uint32_t InventoryAPI(std::uint32_t server,api::StockItem* out,std::uint32_t capacity);
void Save(SKSE::SerializationInterface* io);
void Load(SKSE::SerializationInterface* io);
}
