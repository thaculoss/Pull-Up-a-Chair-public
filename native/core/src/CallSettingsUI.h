#pragma once

namespace inn::call::ui {
// Registers the General page, including language, call controls and table cleanup.
// A missing optional settings API leaves dialogue and the ordering UI intact.
bool Initialize();
void SetLanguageChangedHandler(void (*handler)());
// Cancel key capture on load/new game, and when this page is no longer visible.
void Reset();
}
