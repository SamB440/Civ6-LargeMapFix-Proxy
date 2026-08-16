#pragma once
#include <windows.h>

// Which executable are we loaded into?
//
// Civ VI ships TWO executables -- CivilizationVI.exe (DirectX 11) and
// CivilizationVI_DX12.exe -- and its launcher asks which to run. Both import version.dll,
// so this proxy loads into either, but every patch site here is an address in the DX12
// binary and the two are different code (.text is 0xFA3FCC against 0xF94E2C). On DX11
// every site fails its verification and nothing is applied, which is correct but says
// nothing useful: "320 sites do not match this build" reads like a game update.
//
// So the host executable is named in the log, and the mismatch messages say which of the
// two situations it is. This is diagnostics only -- it never gates a patch. Verification
// against the loaded image stays the authority, so renaming the executable or running a
// non-Steam build degrades to a slightly less specific message rather than a refusal.
//
// Raw Win32 only: called from DllMain under the loader lock, where the CRT is not safe.
namespace HostExe {

// The process executable's file name without its directory, as ASCII. Always NUL
// terminated; empty if the path cannot be read.
inline const char* Name(char* out, size_t cap) {
    if (!out || !cap) return out;
    out[0] = 0;

    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (!n || n >= MAX_PATH) return out;

    const wchar_t* base = path;
    for (const wchar_t* p = path; *p; ++p)
        if (*p == L'\\' || *p == L'/') base = p + 1;

    size_t i = 0;
    for (; base[i] && i + 1 < cap; ++i)
        out[i] = (base[i] > 0 && base[i] < 128) ? (char)base[i] : '?';
    out[i] = 0;
    return out;
}

// True if the host is CivilizationVI_DX12.exe, the build these patches target.
// Case-insensitive over ASCII: the file system is case-insensitive, so a launcher or
// shortcut may hand us any casing.
inline bool IsDX12() {
    char name[MAX_PATH];
    Name(name, sizeof(name));

    const char* want = "CivilizationVI_DX12.exe";
    size_t i = 0;
    for (; want[i] && name[i]; ++i) {
        char a = name[i], b = want[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b) return false;
    }
    return want[i] == 0 && name[i] == 0;   // both ended together: a full match
}

}  // namespace HostExe
