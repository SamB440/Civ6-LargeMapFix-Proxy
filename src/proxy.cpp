// version.dll proxy - loads the large-map fixes into CivilizationVI_DX12.exe.
//
// WHY version.dll
//
// The game imports it directly, and it is NOT in the KnownDLLs registry list, so a copy
// sitting beside the executable wins the module search order and is loaded during
// process initialisation -- before anything renders. It also has nothing to do with
// graphics, so it will not collide with ReShade, overlays, or a d3d12/dxgi proxy.
//
// Verified against this build of CivilizationVI_DX12.exe: it imports exactly six of the
// seventeen exports (GetFileVersionInfoW, GetFileVersionInfoExW, GetFileVersionInfoSizeW,
// GetFileVersionInfoSizeExW, VerQueryValueA, VerQueryValueW). All seventeen are forwarded
// anyway, because other modules in the process import this DLL too.
//
// HOW THE FORWARDING WORKS
//
// Every export is a linker *forwarder* to `version_orig.dll`, which install.bat places
// beside this DLL as a copy of C:\Windows\System32\version.dll. The loader resolves a
// forwarded export by loading that module and looking the name up in it, so the real
// implementation runs with the real signature.
//
// This is deliberately not a set of hand-written wrapper functions. Sixteen of the
// seventeen exports are documented, but GetFileVersionInfoByHandle is not -- a wrapper
// for it would be a guessed signature, and a wrong guess corrupts the stack of whoever
// calls it. Forwarding is exact for all seventeen and cannot get a signature wrong.
//
// Ordinals are pinned to match the real DLL so that anything importing by ordinal rather
// than by name still resolves correctly.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "LargeMapFix.h"
#include "Widen.h"

#pragma comment(linker, "/export:GetFileVersionInfoA=version_orig.GetFileVersionInfoA,@1")
#pragma comment(linker, "/export:GetFileVersionInfoByHandle=version_orig.GetFileVersionInfoByHandle,@2")
#pragma comment(linker, "/export:GetFileVersionInfoExA=version_orig.GetFileVersionInfoExA,@3")
#pragma comment(linker, "/export:GetFileVersionInfoExW=version_orig.GetFileVersionInfoExW,@4")
#pragma comment(linker, "/export:GetFileVersionInfoSizeA=version_orig.GetFileVersionInfoSizeA,@5")
#pragma comment(linker, "/export:GetFileVersionInfoSizeExA=version_orig.GetFileVersionInfoSizeExA,@6")
#pragma comment(linker, "/export:GetFileVersionInfoSizeExW=version_orig.GetFileVersionInfoSizeExW,@7")
#pragma comment(linker, "/export:GetFileVersionInfoSizeW=version_orig.GetFileVersionInfoSizeW,@8")
#pragma comment(linker, "/export:GetFileVersionInfoW=version_orig.GetFileVersionInfoW,@9")
#pragma comment(linker, "/export:VerFindFileA=version_orig.VerFindFileA,@10")
#pragma comment(linker, "/export:VerFindFileW=version_orig.VerFindFileW,@11")
#pragma comment(linker, "/export:VerInstallFileA=version_orig.VerInstallFileA,@12")
#pragma comment(linker, "/export:VerInstallFileW=version_orig.VerInstallFileW,@13")
#pragma comment(linker, "/export:VerLanguageNameA=version_orig.VerLanguageNameA,@14")
#pragma comment(linker, "/export:VerLanguageNameW=version_orig.VerLanguageNameW,@15")
#pragma comment(linker, "/export:VerQueryValueA=version_orig.VerQueryValueA,@16")
#pragma comment(linker, "/export:VerQueryValueW=version_orig.VerQueryValueW,@17")

BOOL APIENTRY DllMain(HMODULE self, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(self);
        // The resource-list relocation is the one thing that CANNOT go on the worker
        // thread: the object's constructor is one of the exe's own CRT initialisers
        // and runs as soon as this returns, so a thread would lose the race and the
        // constructor would build the old layout. Apply() is written for this -- no
        // file I/O, no threads, no CRT dependencies, and its log is buffered until
        // the worker can write it.
        Widen::Apply(self);
        // Install() only calls CreateThread. Doing anything more here would run under
        // the loader lock during process init, which is a deadlock waiting to happen.
        LargeMapFix::Install(self);
    }
    return TRUE;
}
