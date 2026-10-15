#pragma once
namespace inn::service::reservation {
// Called on the game's DataLoaded message, after forms and their final overrides
// are loaded. Prefixes only the reviewed ambient event candidate conditions.
void Initialize();
}
