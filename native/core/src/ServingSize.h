#pragma once
#include "ServingSizePolicy.h"
namespace inn::serving {
void LoadSettings();
Settings GetSettings();
bool SaveSettings(Settings value);
bool InitializeSettingsPage();
bool RefreshSettingsPageLabel();
}
