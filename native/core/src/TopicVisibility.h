#pragma once
namespace inn::topics {
// Add a visibility condition to the two owned cleanup INFO records only.
// The original staff/seating conditions and their dialogue fragments remain.
void Initialize();
void QueueSync();
// Called on the game task thread; one snapshot per conversation/seat.
void TraceDialogueContext();
}
