// Widen - raise the renderer's resource handle space from 32768 to 65534.
//
// WHY THIS IS POSSIBLE AT ALL
//
// An earlier version of this project recorded that the 0x8000 ceiling "cannot be
// raised because the backing storage is inline in the object at fixed offsets".
// The first half of that is true and the second half does not follow. Measured,
// not assumed:
//
//   PartitionedResourceList hands out uint16 handles meet-in-the-middle. Statics
//   count UP from 1, dynamics count DOWN from 0x8000:
//
//       0x140940642   cmp ecx, 0x8000        ; the gate
//       0x14094064d   mov edi, 0x8000        ; the dynamic origin
//       0x140940653   sub di, ax
//
//   so 0x8000 is not the limit of the handle FORMAT, it is half of it. Every
//   consumer zero-extends before doing arithmetic (`movzx ecx,di`, then 64-bit
//   `add rcx,rcx`), every scan loop compares bounds in 32 bits
//   (`movzx eax,bx ; cmp eax,[rsi] ; jb`) so `inc bx` cannot wrap, and nothing
//   anywhere masks the value. A 59-site audit of every 16-bit-destination
//   arithmetic instruction in the renderer band found no truncation that matters.
//   Raising the origin and the gate to 0xFFFF therefore changes no data format.
//
// WHAT ACTUALLY BLOCKS IT
//
// The object is a STATIC at module+0x6db3840, 0x6a1180 bytes (~6.9 MB), so there
// is no allocation size to bump -- the count is baked into the object's layout:
//
//   +0x00000  next-handle counters (static in the low dword, dynamic in the high)
//   +0x00080  refcounts    0x8000 x 4   = 0x20000
//   +0x20080  live count, then 3 x 0x428 substructures
//   +0x20d00  main array   0x8000 x 16  = 0x80000   (indexed scale 8, handle x2)
//   +0xa0d00  tail fields
//   +0xa1180  records      0x8000 x 192 = 0x600000  (index x192 via lea+shl 6)
//   +0x6a1180 end -- the next global starts here and is separately referenced
//
// THREE arrays scale, not two. The record array was originally read as "3 x
// 64-byte frame buffers" from its access pattern, and that was wrong by four
// orders of magnitude: it is 0x8000 entries of 192 bytes. The mistake is worth
// recording because of HOW it happened -- a runtime-indexed array only ever
// exposes its BASE displacement, so a displacement scan can see where it starts
// and never how far it runs. Sizes come from the bulk-zero calls, which state
// them outright. Infer layout from initialisers, not from access patterns.
//
// Doubling the three moves every field above them, which is why this is 320 edits
// rather than 4: 140 register displacements, 105 rip-relative references, 31
// image-relative references, 39 size/limit/displacement immediates, and 5 unwind
// fields. Every code edit is a 4-byte field inside an instruction that
// keeps its length -- a disp32 stays a disp32 whatever value it holds -- so
// nothing is relocated, no instruction is stolen, and no trampoline exists.
//
// Six sites state one of the three sizes for THIS object; seven more state the
// same constants for sibling resource lists (fn 0x140971840 -> 0x14673d700 and
// others, fn 0x1409742b0 whose main array is at +0x20380, fns 0x1409744b0 /
// 0x140974410 / 0x14098e2b0 with different layouts). Only the six are touched.
//
// HOW THE SITES WERE CHOSEN, AND WHY NOT BY RANGE
//
// The obvious approach -- rewrite every displacement in the renderer band that
// falls in the object's range -- is unsound, and provably so on this binary.
// There are ten byte-identical refcount-release functions at 0x14094d4d0 + 0xd0
// stride; they are one template instantiated over ten DIFFERENT objects. Three of
// them touch +0x20080 and +0x20088 on objects at 0x14673d680, 0x146c1e440 and
// elsewhere. A range scan rewrites those and corrupts three unrelated structures.
//
// So each site is included only when the object pointer is provably in that
// operand, traced from a `lea reg,[rip -> the object]` through register copies
// and `lea reg,[A+B]` folding, with every function whose pointer arrives as a
// parameter resolved by checking all of its callers. Of the ten release families
// exactly one -- 0x14094dc20, whose three callers all pass our object -- is in.
//
// One more trap: the object legitimately appears in the INDEX slot, because the
// +0xa1180 record accesses are [rdi + r12 + 0xa118a] with r12 holding it. But
// only ever with scale 1. Accepting a scaled index instead swept up
// `lea rcx,[rax*8 + 0x675da00]` in 0x14096e100, which is a different table
// entirely. Index-slot matches therefore require scale 1.
//
// WHY A GUARD PAGE
//
// The provenance trace is a linear scan; it does not model control flow, so a
// register marked object-derived in one block is still marked in a block reached
// by a branch where it holds something else. Both over- and under-inclusion are
// therefore possible in principle, and a missed site would read the OLD address
// and silently corrupt renderer memory. That is the one failure mode worth
// engineering against, because it is invisible.
//
// So after relocating, the old region is made PAGE_NOACCESS. Any site that still
// reaches the old object faults IMMEDIATELY, and the handler names the exact
// offset and instruction instead of leaving a corrupted frame to crash somewhere
// unrelated ten seconds later. Silent corruption becomes a precise bug report.
//
// Note the one gap this leaves, because it bit: a missed reference inside the
// unguardable head sliver does NOT fault. The spinlock at +0x40 lives there, and
// a CAS against poison never succeeds, so the symptom is an infinite spin rather
// than a crash -- and a spin writes nothing, so the poison check cannot see it
// either. That is why the static side proves the stronger property: after
// applying the plan, the generator re-scans ALL of .text and asserts that ZERO
// references to the old object survive. Completeness is verified, not hoped
// for; the guard is the backstop, not the primary control.
//
// Neither end of the object is page-aligned, and both end pages are shared with
// neighbours that are genuinely referenced -- 0x146db3780..0x3830 below it, and
// the next global at exactly OBJ+0x6a1180 above it. So the guard covers only the
// wholly-owned pages (6.62 MB of 6.9), and the two slivers either side are filled
// with 0xCD and polled instead: a missed site that writes there is still caught,
// just on a poll rather than instantly. Both ends matter -- rounding the upper
// bound UP instead of down would protect the neighbour and break it.
//
// MODES (CIVFIX_WIDEN)
//
//   0 / unset  off. Nothing is touched.
//   1          relocate and guard, but leave the limit at 32768. The game should
//              behave EXACTLY as before -- same handle count, same everything,
//              just a moved object. Any fault at all means a missed site. This is
//              the validation mode and it is the one to run first.
//   2          relocate, guard, and raise the limit to 65534.
//
// Mode 1 is the whole point: it separates "did I find every reference" from "does
// a bigger limit work", so a failure in either has an unambiguous cause.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <cstdlib>

#include "Widen.h"
#include "WidenTable.h"
#include "HostExe.h"

namespace Widen {
namespace {

// The two immediates that ARE the limit, as opposed to the two that merely size
// the constructor's memsets. Mode 1 applies the memsets (the layout really did
// change) but not these.
const uint32_t RVA_GATE   = 0x940642;   // AddDynamicResource: cmp ecx, 0x8000
const uint32_t RVA_ORIGIN = 0x94064D;   // AddDynamicResource: mov edi, 0x8000
// AddStaticResource is a separate function with its own gate on the SAME
// combined count. Widening only the dynamic one let dynamics reach 65535 while
// statics stayed capped at 32768 -- which the game logged, then turned into a
// CreatePlacedResource failure and a NULL deref in its own rollback path.
const uint32_t RVA_GATE_S = 0x941561;   // AddStaticResource:  cmp ecx, 0x8000
// The handle-space top, replicated as `0x8000 - dynamic_count` in three more
// places. These belong with the gate and the origin, not with the array sizes:
// they only make sense once the origin has actually moved, so mode 1 skips them
// and stays behaviour-identical.
const uint32_t RVA_TOP[] = { 0x96A3E1, 0x96B55F, 0x96C9AB };
// NumDescriptors for the four D3D12 descriptor heaps that are indexed by the raw
// handle (one RTV, three CBV_SRV_UAV, all created in 0x1409515ba). They live
// outside the object, so mode 1 must leave them alone; but the moment the origin
// moves to 0xFFFF the first dynamic resource indexes past the end of a
// 0x8000-descriptor heap and the driver faults. 0x8000 -> 0x10000.
const uint32_t RVA_HEAP[] = { 0x951DC7, 0x951E26, 0x951E4D, 0x951E9B };
// The stack frame of 0x14095FDE0, which holds a 2-bits-per-handle bitmap as a
// LOCAL. Growing 0x2058 -> 0x4058 doubles the bitmap and its memset in one edit
// (the clear length is `lea edx,[rax-0x58]`), and everything at or above 0x2030
// shifts by +0x2000. Mode 1 must not touch any of it: a half-shifted frame is
// far worse than an unwidened one. Contiguous range, so a bounds test covers it
// -- the function body is 0x95FDE0..0x96013F and nothing else lives in there.
const uint32_t FRAME_LO = 0x95FDE0, FRAME_HI = 0x960140;

bool IsLimitEdit(uint32_t rva) {
    if (rva == RVA_GATE || rva == RVA_ORIGIN || rva == RVA_GATE_S) return true;
    for (int i = 0; i < (int)(sizeof(RVA_TOP) / sizeof(RVA_TOP[0])); ++i)
        if (rva == RVA_TOP[i]) return true;
    for (int i = 0; i < (int)(sizeof(RVA_HEAP) / sizeof(RVA_HEAP[0])); ++i)
        if (rva == RVA_HEAP[i]) return true;
    if (rva >= FRAME_LO && rva < FRAME_HI) return true;
    return false;
}

const BYTE POISON = 0xCD;

// Log lines are buffered: Apply() runs under the loader lock during process
// initialisation, where opening a file is a deadlock risk. The worker thread
// drains this once the log exists.
const int LOGCAP = 32 * 1024;
char          g_buf[LOGCAP];
volatile LONG g_len = 0;
LONG          g_flushed = 0;

BYTE* g_mod = nullptr;
BYTE* g_new = nullptr;          // the relocated object
bool  g_active = false;
int   g_mode = 0;
int   g_limit = 0x8000;

volatile LONG g_misses = 0;
volatile LONG g_lifted = 0;     // guard temporarily off after a miss
DWORD         g_liftAt = 0;
volatile LONG g_poisonHits = 0;

// The mode, read from CivFixWiden.txt beside this DLL.
//
// An environment variable is the wrong control for this: Steam launches the game
// with the environment STEAM itself inherited, so changing the variable does
// nothing until Steam is restarted -- which silently produced two more mode-1
// runs that looked like fresh failures. A file is read at every start, so the
// mode can be changed with the launcher already open.
//
// Raw Win32 only. This runs under the loader lock during process init, where the
// CRT's stream layer is not safe to touch; CreateFileW/ReadFile are not.
// Returns -1 if the file is absent or holds no digit.
int ReadModeFile(HMODULE self) {
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(self, path, MAX_PATH);
    if (!n || n >= MAX_PATH) return -1;
    wchar_t* slash = nullptr;
    for (wchar_t* p = path; *p; ++p)
        if (*p == L'\\') slash = p;
    if (!slash) return -1;
    const wchar_t* name = L"CivFixWiden.txt";
    size_t room = MAX_PATH - (size_t)(slash + 1 - path);
    size_t i = 0;
    for (; name[i] && i + 1 < room; ++i) slash[1 + i] = name[i];
    slash[1 + i] = 0;

    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return -1;
    char b[16] = { 0 };
    DWORD got = 0;
    BOOL ok = ReadFile(h, b, sizeof(b) - 1, &got, nullptr);
    CloseHandle(h);
    if (!ok || !got) return -1;
    for (DWORD k = 0; k < got; ++k)
        if (b[k] >= '0' && b[k] <= '9') return b[k] - '0';
    return -1;
}

void buf(const char* fmt, ...) {
    char line[512];
    va_list ap; va_start(ap, fmt);
    int n = _vsnprintf_s(line, sizeof(line), _TRUNCATE, fmt, ap);
    va_end(ap);
    if (n <= 0) return;
    // Store the terminator too, so the buffer is a run of separate C strings.
    // Without it the whole buffer reads as ONE string and the drain truncates it
    // at its own scratch size -- which mangled the first run's log and would
    // happily have swallowed a MISSED SITE report.
    LONG at = _InterlockedExchangeAdd(&g_len, n + 1);
    if (at + n + 1 < LOGCAP) memcpy(g_buf + at, line, n + 1);
    else _InterlockedExchangeAdd(&g_len, -(n + 1));   // full: drop, keep the index sane
    OutputDebugStringA(line);
}

void Drain(void (*sink)(const char*)) {
    LONG end = g_len;
    while (g_flushed < end) {
        const char* p = g_buf + g_flushed;
        size_t max = (size_t)(end - g_flushed);
        size_t n = strnlen(p, max);
        // n == 0: space reserved but not yet filled. n == max: the line is still
        // being written. Either way, leave it for the next poll.
        if (n == 0 || n >= max) break;
        sink(p);                       // already NUL-terminated, no copy needed
        g_flushed += (LONG)n + 1;
    }
}

bool Poke32(BYTE* at, int32_t v) {
    DWORD old;
    if (!VirtualProtect(at, 4, PAGE_EXECUTE_READWRITE, &old)) return false;
    *(int32_t*)at = v;
    VirtualProtect(at, 4, old, &old);
    return true;
}

// The unwind records live in .xdata, which is read-only and is NOT the section
// Poke32's callers assume, so this makes no assumption about current protection.
bool Poke16(BYTE* at, uint16_t v) {
    DWORD old;
    if (!VirtualProtect(at, 2, PAGE_READWRITE, &old)) return false;
    *(uint16_t*)at = v;
    VirtualProtect(at, 2, old, &old);
    return true;
}

// A block within +-2GB of the module, so every rip-relative disp32 still reaches.
void* AllocNearModule(BYTE* origin, SIZE_T bytes) {
    SYSTEM_INFO si; GetSystemInfo(&si);
    const uintptr_t gran = si.dwAllocationGranularity;
    uintptr_t base = (uintptr_t)origin;
    for (uintptr_t d = gran; d < 0x60000000ULL; d += gran)
        for (int dir = 0; dir < 2; ++dir) {
            uintptr_t a = (dir ? base + d : base - d) & ~(uintptr_t)(gran - 1);
            void* p = VirtualAlloc((void*)a, bytes, MEM_COMMIT | MEM_RESERVE,
                                   PAGE_READWRITE);
            if (p) return p;
        }
    return nullptr;
}

// Any access that still reaches the old object is a site the analysis missed.
// Report it exactly, then lift the guard so the run continues and other misses
// can surface; the worker re-arms it a couple of seconds later.
LONG CALLBACK Veh(EXCEPTION_POINTERS* ep) {
    const EXCEPTION_RECORD* er = ep->ExceptionRecord;
    if (er->ExceptionCode != EXCEPTION_ACCESS_VIOLATION ||
        er->NumberParameters < 2)
        return EXCEPTION_CONTINUE_SEARCH;

    const uintptr_t addr = (uintptr_t)er->ExceptionInformation[1];
    const uintptr_t lo = (uintptr_t)g_mod + GUARD_RVA;
    const uintptr_t hi = (uintptr_t)g_mod + GUARD_END;
    if (addr < lo || addr >= hi) return EXCEPTION_CONTINUE_SEARCH;

    const uintptr_t objBase = (uintptr_t)g_mod + OBJ_RVA;
    LONG k = _InterlockedIncrement(&g_misses);
    if (k <= 32)
        buf("[widen] *** MISSED SITE #%ld *** RIP module+0x%llX %s OLD object "
            "+0x%llX (addr %p) - this reference was not rewritten\n",
            k, (unsigned long long)((BYTE*)ep->ContextRecord->Rip - g_mod),
            er->ExceptionInformation[0] == 1 ? "WROTE" : "READ",
            (unsigned long long)(addr - objBase), (void*)addr);

    DWORD old;
    if (VirtualProtect((void*)lo, hi - lo, PAGE_READWRITE, &old)) {
        g_liftAt = GetTickCount();
        _InterlockedExchange(&g_lifted, 1);
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

}  // namespace

bool Active() { return g_active; }
int  Limit()  { return g_limit; }

void Apply(HMODULE self) {
    char mode[16] = { 0 };
    GetEnvironmentVariableA("CIVFIX_WIDEN", mode, sizeof(mode));
    const int fromEnv = atoi(mode);
    const int fromFile = ReadModeFile(self);
    // The file wins: it is the one that can be changed without restarting Steam.
    g_mode = (fromFile >= 0) ? fromFile : fromEnv;
    buf("[widen] mode %d (CivFixWiden.txt=%s, CIVFIX_WIDEN=%s)\n", g_mode,
        fromFile >= 0 ? (fromFile == 0 ? "0" : (fromFile == 1 ? "1" : "2")) : "absent",
        mode[0] ? mode : "unset");
    if (g_mode <= 0) {
        buf("[widen] off. Put 1 in CivFixWiden.txt to relocate and guard with the "
            "limit unchanged (run this first); 2 also raises it to 65534.\n");
        return;
    }

    g_mod = (BYTE*)GetModuleHandleW(nullptr);
    if (!g_mod) { buf("[widen] no main module - NOT APPLIED\n"); return; }

    BYTE* obj = g_mod + OBJ_RVA;

    // The constructor at 0x140973ff0 is a CRT initialiser and must build the NEW
    // layout. If the object is already non-zero we are too late, and relocating
    // now would strand live state in the old block.
    for (int i = 0; i < 128; ++i)
        if (obj[i] != 0) {
            buf("[widen] object at module+0x%X is already initialised (byte %d = "
                "0x%02X) - too late to relocate, NOT APPLIED\n", OBJ_RVA, i, obj[i]);
            return;
        }

    // ---- verify every field before writing a single one -------------------
    // The `oldv` values are what makes this build-specific. Checking every one
    // first means a different build of the game fails cleanly rather than
    // half-patching itself into an unrecoverable state.
    int bad = 0;
    const Edit* sets[4] = { kDisp, kRip, kImm, kImgRel };
    const int   counts[4] = { kNDisp, kNRip, kNImm, kNImgRel };
    const char* names[4] = { "disp", "rip", "imm", "imgrel" };
    for (int s = 0; s < 4; ++s)
        for (int i = 0; i < counts[s]; ++i) {
            const Edit& e = sets[s][i];
            int32_t cur = *(int32_t*)(g_mod + e.rva + e.field);
            if (cur != e.oldv) {
                if (++bad <= 8)
                    buf("[widen] MISMATCH %s[%d] at module+0x%X+%d: found 0x%08X, "
                        "expected 0x%08X\n", names[s], i, e.rva, e.field, cur, e.oldv);
            }
        }
    // The .xdata unwind fields are 2-byte and have no instruction to pin to.
    for (int i = 0; i < kNUnwind; ++i) {
        const Unwind& u = kUnwind[i];
        uint16_t cur = *(uint16_t*)(g_mod + u.rva);
        if (cur != u.oldv) {
            if (++bad <= 8)
                buf("[widen] MISMATCH unwind[%d] at module+0x%X: found 0x%04X, "
                    "expected 0x%04X\n", i, u.rva, cur, u.oldv);
        }
    }
    const int kNAll = kNDisp + kNRip + kNImm + kNImgRel + kNUnwind;
    if (bad) {
        // Two very different causes, and the log should not make the reader guess.
        if (!HostExe::IsDX12()) {
            char exe[MAX_PATH];
            HostExe::Name(exe, sizeof(exe));
            buf("[widen] %d of %d sites do not match - NOTHING APPLIED. *** WRONG "
                "EXECUTABLE: this process is %s *** Civ VI ships two executables with "
                "different code, and these patches target CivilizationVI_DX12.exe. "
                "Relaunch and choose DirectX 12.\n", bad, kNAll, exe);
        } else {
            buf("[widen] %d of %d sites do not match this build - NOTHING APPLIED. "
                "The patch table was generated for a different CivilizationVI_DX12.exe, "
                "so the game has most likely been updated.\n", bad, kNAll);
        }
        return;
    }
    buf("[widen] verified all %d sites against the loaded image\n", kNAll);

    // ---- allocate the new object ------------------------------------------
    g_new = (BYTE*)AllocNearModule(g_mod, NEW_SPAN);
    if (!g_new) {
        buf("[widen] could not reserve 0x%X bytes within +-2GB of %p - NOT APPLIED\n",
            NEW_SPAN, g_mod);
        return;
    }

    // Every rip-relative reference must still reach it. Checked before any write,
    // for the same reason as above.
    for (int i = 0; i < kNRip; ++i) {
        const Edit& e = kRip[i];
        intptr_t d = (intptr_t)(g_new + e.newv) - (intptr_t)(g_mod + e.rva + e.len);
        if (d < INT32_MIN || d > INT32_MAX) {
            buf("[widen] block %p is out of rip32 range of module+0x%X - NOT APPLIED\n",
                g_new, e.rva);
            VirtualFree(g_new, 0, MEM_RELEASE);
            g_new = nullptr;
            return;
        }
    }
    // Image-relative references are displacements off the MODULE BASE, so what
    // has to fit is the block's distance from the module, not from each site.
    {
        intptr_t d = (intptr_t)(g_new + NEW_SPAN) - (intptr_t)g_mod;
        intptr_t d0 = (intptr_t)g_new - (intptr_t)g_mod;
        if (kNImgRel && (d < INT32_MIN || d > INT32_MAX ||
                         d0 < INT32_MIN || d0 > INT32_MAX)) {
            buf("[widen] block %p is out of int32 range of the module base %p, which "
                "the %d image-relative references need - NOT APPLIED\n",
                g_new, g_mod, kNImgRel);
            VirtualFree(g_new, 0, MEM_RELEASE);
            g_new = nullptr;
            return;
        }
    }

    // ---- apply -------------------------------------------------------------
    int done = 0, failed = 0, skipped = 0;

    for (int i = 0; i < kNDisp; ++i) {
        const Edit& e = kDisp[i];
        if (Poke32(g_mod + e.rva + e.field, e.newv)) ++done; else ++failed;
    }
    for (int i = 0; i < kNRip; ++i) {
        const Edit& e = kRip[i];
        int32_t d = (int32_t)((intptr_t)(g_new + e.newv) -
                              (intptr_t)(g_mod + e.rva + e.len));
        if (Poke32(g_mod + e.rva + e.field, d)) ++done; else ++failed;
    }
    for (int i = 0; i < kNImgRel; ++i) {
        const Edit& e = kImgRel[i];
        // effective address is module_base + <disp>, so the new displacement is
        // the block's offset from the module base plus the field's new offset
        int32_t d = (int32_t)((intptr_t)(g_new + e.newv) - (intptr_t)g_mod);
        if (Poke32(g_mod + e.rva + e.field, d)) ++done; else ++failed;
    }
    for (int i = 0; i < kNImm; ++i) {
        const Edit& e = kImm[i];
        if (IsLimitEdit(e.rva) && g_mode < 2) {   // validation mode: layout only
            ++skipped;
            continue;
        }
        if (Poke32(g_mod + e.rva + e.field, e.newv)) ++done; else ++failed;
    }
    // Unwind metadata for the one function whose stack frame grows. Mode 2 only:
    // in mode 1 the frame is untouched, so rewriting these would DESCRIBE a frame
    // that does not exist -- strictly worse than leaving them alone.
    for (int i = 0; i < kNUnwind; ++i) {
        const Unwind& u = kUnwind[i];
        if (g_mode < 2) { ++skipped; continue; }
        if (Poke16(g_mod + u.rva, u.newv)) ++done; else ++failed;
    }

    FlushInstructionCache(GetCurrentProcess(), g_mod, 0x1000000);

    if (failed) {
        buf("[widen] %d of %d writes FAILED - the image is now partially patched "
            "and this process is not safe to play. Close the game.\n",
            failed, done + failed);
        return;
    }

    g_limit = (g_mode >= 2) ? 0xFFFF : 0x8000;
    g_active = true;

    buf("[widen] mode %d: relocated the resource list from module+0x%X to %p "
        "(0x%X -> 0x%X bytes)\n", g_mode, OBJ_RVA, g_new, OLD_SPAN, NEW_SPAN);
    buf("[widen]   %d edits applied (%d disp, %d rip, %d imgrel, %d imm), %d skipped, "
        "handle limit = %d\n", done, kNDisp, kNRip, kNImgRel, kNImm - skipped,
        skipped, g_limit);
    if (g_mode < 2)
        buf("[widen]   VALIDATION RUN: the limit is unchanged, so the game should "
            "behave exactly as before. Any fault below is a reference the analysis "
            "missed, not a symptom of the larger limit.\n");

    // ---- guard the old region ---------------------------------------------
    memset(g_mod + POISON1_RVA, POISON, POISON1_LEN);
    memset(g_mod + POISON2_RVA, POISON, POISON2_LEN);

    DWORD old;
    if (VirtualProtect(g_mod + GUARD_RVA, GUARD_END - GUARD_RVA,
                       PAGE_NOACCESS, &old)) {
        buf("[widen]   guard: module+0x%X..0x%X is now NOACCESS (%.2f MB); the %d bytes "
            "below and %d above share a page with other statics and are poisoned "
            "with 0x%02X instead\n",
            GUARD_RVA, GUARD_END, (GUARD_END - GUARD_RVA) / 1048576.0,
            POISON1_LEN, POISON2_LEN, POISON);
    } else {
        buf("[widen]   WARNING: could not protect the old region (%lu). A missed "
            "reference will corrupt silently instead of faulting.\n", GetLastError());
    }

    AddVectoredExceptionHandler(1, Veh);
}

void FlushLog(void (*sink)(const char*)) { Drain(sink); }

void Poll(void (*sink)(const char*)) {
    if (g_active) {
        // Re-arm after a miss lifted the guard, so later misses are still caught.
        if (g_lifted && GetTickCount() - g_liftAt > 2000) {
            DWORD old;
            if (VirtualProtect(g_mod + GUARD_RVA, GUARD_END - GUARD_RVA,
                               PAGE_NOACCESS, &old))
                _InterlockedExchange(&g_lifted, 0);
        }

        // The two unguardable slivers: check the poison is intact.
        if (!g_poisonHits) {
            const uint32_t rvas[2] = { POISON1_RVA, POISON2_RVA };
            const uint32_t lens[2] = { POISON1_LEN, POISON2_LEN };
            for (int s = 0; s < 2 && !g_poisonHits; ++s) {
                const BYTE* p = g_mod + rvas[s];
                for (uint32_t i = 0; i < lens[s]; ++i)
                    if (p[i] != POISON) {
                        _InterlockedIncrement(&g_poisonHits);
                        buf("[widen] *** POISON DISTURBED *** old object +0x%X changed "
                            "(0x%02X). Something still writes there -- a missed "
                            "reference in one of the two regions too close to a "
                            "neighbour to guard.\n",
                            rvas[s] - OBJ_RVA + i, p[i]);
                        break;
                    }
            }
        }
    }
    Drain(sink);
}

}  // namespace Widen
