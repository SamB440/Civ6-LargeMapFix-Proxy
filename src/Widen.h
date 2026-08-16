#pragma once
#include <windows.h>

// Widening the renderer's resource handle space from 32768 to 65534.
//
// This is the only fix in this project that rewrites the game's own data layout
// rather than guarding a call, so it runs under its own switch and its own
// verification. See Widen.cpp for the full rationale.
namespace Widen {

// Applied from DllMain, synchronously, BEFORE the game's CRT initialisers run --
// the resource list's constructor is one of them, and it must build the new
// layout, not the old one. Does no file I/O and creates no threads, so it is
// safe under the loader lock; log lines are buffered and flushed later.
void Apply(HMODULE self);

// Emit everything Apply() buffered, plus the current guard state. Called from the
// worker thread once the log file exists.
void FlushLog(void (*sink)(const char*));

// Called periodically from the worker thread: re-arms the guard after a miss and
// checks the poisoned header window for stray writes.
void Poll(void (*sink)(const char*));

// True if the widening actually installed. The rest of the fix pack reads this so
// its own limit reporting matches reality.
bool Active();

// The handle limit now in force: 0x8000 if not widened, 0xFFFF if it was.
int  Limit();

}  // namespace Widen
