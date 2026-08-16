#pragma once
#include <windows.h>

// Large-map crash fixes for CivilizationVI_DX12.exe.
//
// These patch the game executable's own .text and .data in memory. Nothing is written to
// disk, nothing touches GameCore or the game's mod pipeline, and no state is shared with
// anything else in the process -- this module needs only to be loaded into it.
//
// See LARGE-MAP-FIX.md for the full diagnosis of each crash and why each patch is narrow.
namespace LargeMapFix {
    // Call once from DllMain on DLL_PROCESS_ATTACH. Returns immediately; the work runs
    // on a worker thread that waits for the game module to be ready.
    void Install(HMODULE self);
}
