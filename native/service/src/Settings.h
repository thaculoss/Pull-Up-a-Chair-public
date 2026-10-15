#pragma once
#include "Localization.h"
#include <atomic>
#include <cstdint>
namespace innservice {
struct Settings {
    std::atomic_int key{37};std::atomic_uint chord{0x300};
    float radius{1800},arrival{145},approachTimeout{30},readyTimeout{45},conversationTimeout{240};
    bool autoTalk{true};
};
inline Settings settings;
inline std::atomic_uint bindingRevision{1};
void LoadSettings();
const char* MenuSection();
void ApplyMenuLanguage(bool value);
bool SetKeyboardBinding(int key);
bool SetGamepadBinding(std::uint32_t mask);
bool ResetBindings();
std::string KeyboardName(int key);
std::string GamepadName(std::uint32_t mask);
bool BlockingMenus();
inline std::atomic_bool chinese{};
inline const char* Text(std::string_view key){
    // Bridge notices have built-in translations so old installed locale packs remain usable.
    if(key=="core_integrated")return chinese?"已连接点餐核心；请在点餐核心中设置招呼按键。":"Connected to Inn Ordering. Configure call controls in the ordering settings.";
    if(key=="core_unavailable")return chinese?"点餐核心服务接口不可用；独立招呼已停用，请检查两个模块的版本。":"The ordering service interface is unavailable. Standalone calling is disabled; check both module versions.";
    if(key=="core_migration")return chinese?"正在清理旧版服务状态，完成后启用统一招呼。":"Clearing the legacy service state before enabling unified calling.";
    if(key=="core_migration_failed")return chinese?"旧版服务清理尚未完成，招呼保持关闭；点击刷新可重试。":"Legacy service cleanup did not complete. Calling remains disabled; Refresh retries cleanup.";
    if(key=="core_waiting")return chinese?"点餐核心尚未准备好，请稍后再试。":"The ordering engine is not ready yet. Try again shortly.";
    if(key=="core_menu_pending")return chinese?"请在 10 秒内关闭菜单，随后招呼服务员。":"Close the menu within 10 seconds to call the server.";
    if(key=="core_menu_expired")return chinese?"等待关闭菜单已超时，请重新招呼。":"The queued call expired while waiting for menus to close.";
    return inn::T(key);
}
}
