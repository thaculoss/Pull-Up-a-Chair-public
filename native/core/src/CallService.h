#pragma once
#include "CallPolicy.h"
#include <string>
namespace inn::call {
Settings GetSettings();
bool SaveSettings(Settings settings);
bool FrameworkAvailable();
int FrameworkKey();
bool FrameworkHeld();
std::string KeyName(std::uint32_t key);
void Initialize();
void SetOrderGuard(bool (*guard)());
void Reset();
bool Busy();
void RequestFromPrompt();
bool PromptAvailable();
bool InteractionBlocked();
bool RequestExternal();
}
