#pragma once
#include <RE/Skyrim.h>
namespace innservice {
void DrawBindings();
bool CaptureInput(RE::InputEvent* event);
void InstallBindingCapture();
void CancelBindingCapture();
}
