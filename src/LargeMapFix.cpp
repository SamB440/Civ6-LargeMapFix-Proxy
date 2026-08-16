// capfix - fix the Civ VI terrain descriptor index, and cap the table at what it can
// address.
//
// The sections here map onto LARGE-MAP-FIX.md as follows. Fix A, the renderer's resource
// handle space, lives in Widen.cpp -- it is the upstream cause of D, E and F.
//
//   Fix B  the 12->13 bit descriptor index widening   ("index13" below)
//   Fix C  the descriptor cap, a backstop behind B    ("THE CAP" below)
//   Fix D  deferred-destroy queue validation
//   Fix E  collection walker NULL check
//
// The cap (C) came first and is still here, but it is no longer the primary control:
// the index13 section widens the field itself from 12 bits to 13, so the table can hold
// 8192 descriptors rather than 4096, and the cap simply moves up to match.
//
// ROOT CAUSE (see LARGE-MAP-FIX.md, "Fix B -- the 12-bit terrain descriptor index")
//
// The grid owns a descriptor slot allocator at 0x140ac3ce0:
//
//     uint16 Alloc(grid /*rcx*/, const void* desc16 /*rdx*/)
//
// It spin-locks [grid+0x80], scans slots 0..count-1 for a free one (dword0 == 0),
// and if none is free appends to the array at {grid+0x30 ptr, +0x38 cap, +0x40 count},
// 16 bytes per slot. The append was pinned by a hardware write watchpoint on the count
// field:  inc dword ptr [rdi+0x40]  at 0x140ac3d88.  It returns the slot index via
// `movzx eax, bx` -- a FULL 16-BIT value.
//
// Its only caller, the chunk loader at 0x14061a2a9, stores that index straight into a
// record:  mov word ptr [rdx], cx  -- also 16-bit, and with no failure check.
//
// But every consumer of those records truncates to 12 bits:
//
//     movzx edx, word [rax+rcx*2] ; and edx, 0x0FFF ; shl rdx,4 ; add rdx,[rbx+0x30]
//
// So producer and consumer agree only while count <= 4096. On a 180x94 map the count
// reaches 4167 (capacity doubled to 8192), and slots 4096..4166 become unreachable:
// index 4100 is read back as slot 4, whose buffer is a different size, and the vertex
// offset B then runs off the end of it. That is the observed crash:
//
//     0x140ac34dc  movzx eax,[r9+rdx*2+4]   R9=0x137cf4f0c  RDX=0x8629 (=B*5)
//     fault address == R9 + 34345*2 + 4, exactly.
//
// The refcount table at grid+0x48 is indexed by the SAME masked value
// (0x140ac40c1: and ecx,0xfff ; inc dword[rax+rcx*4]), so past 4096 the lifetime
// bookkeeping is corrupted too -- a second failure mode from the same root cause.
//
// THE CAP (Fix C)
//
// Detour Alloc. While count < LIMIT, call straight through: behaviour is bit-identical
// to the unpatched game. Once the table is full we never let it grow. An overflowing
// allocation resolves in this order:
//
//   1. an existing slot holding a byte-identical descriptor  -> return it. Same
//      resource, same buffer, so this is exactly correct.
//   2. otherwise the SINK slot.
//
// The sink is the part that makes this memory-safe. Aliasing an overflow index onto
// some arbitrary live descriptor would reproduce the very bug we are fixing, because B
// is authored for the buffer the record expected. So at startup capfix allocates one
// ordinary slot whose data pointer is a 128 KB zero-filled buffer of our own. B is 13
// bits, records are 10 bytes, so the highest reachable byte is 8191*10+6 = 81916 --
// comfortably inside 128 KB. Every possible B therefore reads zeroes from our buffer
// instead of running off someone else's. The sink's refcount is pinned high so the
// game's own sweeper can never recycle it.
//
// COST: terrain that would have needed a descriptor past LIMIT renders blank rather
// than crashing. Nothing is written to disk; saves are untouched. This is a memory
// patch -- restarting the game reverts it.
//
// With the index13 widening applied LIMIT is 8192, and the highest demand yet measured -- a
// fully revealed ludicrous map -- was ~6055, so the sink is expected to sit idle. The
// `capped` counter in the periodic report is what says whether it did.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <share.h>
#include "Widen.h"
#include "HostExe.h"

// ---------------------------------------------------------------- layout constants

static const int OFF_TAB   = 0x30;   // descriptor array pointer
static const int OFF_CAP   = 0x38;   // capacity (dword)
static const int OFF_COUNT = 0x40;   // live count (dword)  <- the field we cap
static const int OFF_REF   = 0x48;   // refcount array pointer (dword per slot)
static const int OFF_LOCK  = 0x80;   // spinlock: 0 = free, 0x80000000 = held

static const int  DESC_SIZE     = 16;
static const LONG DEFAULT_LIMIT = 4096;        // the 12-bit index field
static const LONG WIDE_LIMIT    = 8192;        // ...once index13 makes it 13 bits
static const SIZE_T SINK_BYTES  = 128 * 1024;  // >= 8191*10+6, the full 13-bit B range
static const DWORD  SINK_PIN    = 0x40000000;  // refcount floor: never swept

// Entry of Alloc @0x140ac3ce0. First instruction is exactly 5 bytes, so a jmp rel32
// detour steals one whole instruction and leaves no partial tail.
//   48 89 5C 24 18   mov qword ptr [rsp+0x18], rbx
static const BYTE SIG_ALLOC[] = {
    0x48, 0x89, 0x5C, 0x24, 0x18, 0x55, 0x57, 0x41,
    0x56, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0xEA };
static const int STOLEN = 5;

// Corroboration: the count increment must sit at +0xA8 and reference [rdi+0x40].
// If this does not match, we found the wrong function or the offsets differ.
static const BYTE CHK_INC[]  = { 0xFF, 0x47, 0x40 };   // inc dword ptr [rdi+0x40]
static const int  CHK_INC_AT = 0xA8;

// ------------------------------------------- Fix A's counterpart: renderer resources
//
// The repair for this limit is in Widen.cpp -- it relocates the object and raises the
// ceiling to 65534. What lives here is the *observation* hook: it counts allocations and
// reports exhaustion, so the log still says whether the widened limit was also reached.
//
// PartitionedResourceList::AddDynamicResource @0x1409405e0 hands out uint16 handles
// from a 15-bit space shared with AddStaticResource: static counts UP from 0, dynamic
// counts DOWN from 0x8000, and the test is (static + dynamic + 1) < 0x8000.
//
//   0x140940642   cmp ecx, 0x8000
//
// On exhaustion it returns 0.
//
// Handle 0 is NOT a collision with "static resource #0" -- an earlier version of this
// file assumed it was, substituted a reserved sink handle, and caused a crash.
// The constructor at 0x140973ff0 ends with:
//
//     0x140974090   mov qword ptr [rbx], 1     ; static count = 1, dynamic count = 0
//
// and AddStaticResource returns the OLD static count as the handle, so the first static
// handle ever issued is 1. **Handle 0 is never issued to anyone: it is the reserved
// "invalid handle" sentinel.** Returning 0 on exhaustion is the game correctly
// signalling failure, not a collision.
//
// Substituting a live-but-unpopulated handle made callers treat a failure as success,
// and they then dereferenced its empty internals:
//
//     0x14096ba47  mov rcx,[rcx+rsi]   ; effective addr = [rbx+0x48] + 0x10, base NULL
//
// So the sink is DISABLED by default and this hook now only observes. Set
// CIVFIX_RES_SINK=1 to re-enable it for experiments.
//
// This block used to end "the limit itself cannot be raised: the backing storage is
// inline in the object at fixed offsets". The premise is right and the conclusion is
// wrong -- inline storage means the count is baked into the object's LAYOUT, not that
// the layout is fixed. Widen.cpp relocates the object and rewrites all 320 references,
// which raises the limit to 65534 with no change to the handle format.
static const BYTE SIG_ADDDYN[] = {
    0x48,0x89,0x5C,0x24,0x08, 0x48,0x89,0x74,0x24,0x10, 0x57,
    0x48,0x81,0xEC,0x80,0x00,0x00,0x00, 0x48,0x8B,0xD9,
    0x33,0xC0, 0xB9,0x01,0x00,0x00,0x00, 0xF0,0x0F,0xB1,0x4B,0x40,
    0x74,0x0B, 0xF3,0x90, 0x33,0xC0, 0xF0,0x0F,0xB1,0x4B,0x40, 0x75,0xF5,
    0x33,0xF6, 0x0F,0xB7,0xFE, 0x39,0xB3,0x50 };
// The prologue is shared with 8 sibling functions; the byte that disambiguates is the
// 0xa1150 displacement at the end of the signature. Corroborate with the limit test.
static const BYTE CHK_CAP[]  = { 0x81, 0xF9, 0x00, 0x80, 0x00, 0x00 };  // cmp ecx,0x8000
static const int  CHK_CAP_AT = 0x62;

static const int OFF_RL_STATIC  = 0x00;      // static resource count
static const int OFF_RL_DYNAMIC = 0x04;      // dynamic resource count
static const int OFF_RL_FREECNT = 0xA1150;   // dynamic free-list count
static const int RL_LIMIT       = 0x8000;    // stock value, from cmp ecx,0x8000.
                                             // Reporting uses Widen::Limit(), which
                                             // tracks whether the gate was widened.

// ------------------------------ Fix D: the deferred-destroy queues' object pointers
//
// The 200x100 crash. Faulting instruction, from the AV logged by this module's own VEH:
//
//   0x14096eeee   movzx ecx, byte ptr [rbx + 0x61]     ; RBX = 0x40000001c
//
// which faults reading 0x...7d -- exactly 0x1c + 0x61, matching the reported address.
//
// RBX is an object pointer taken from a *deferred-destroy queue*. There are two of them,
// same layout, drained back to back by 0x14096e100:
//
//   list A: ptr 0x144491600, count 0x144491610, lock 0x1444917c0, enqueue 0x14096cf90
//   list B: ptr 0x144491618, count 0x144491628,                   enqueue 0x140970590
//
// Entries are 16 bytes, {void* obj, uint32 delay, uint32}. The enqueue appends with
// delay = 3 (`mov dword ptr [rax+rcx*8+8], 3` at +0x12b in both). Each drain pass
// decrements the delay and keeps the entry; at zero the object is finally destroyed:
//
//   mov  ecx, [rbx + r8*8 + 8]   ; delay
//   test ecx, ecx
//   jne  retain                  ; still in flight -> decrement and compact
//   mov  rbx, [rbx + r8*8]       ; delay == 0 -> destroy
//   movzx ecx, byte [rbx + 0x61] ; list A: no NULL check, no validity check
//   mov  eax, [rbx + 0x54]       ; list B at 0x14096ed0f: likewise
//
// So a bad pointer does not fault where it is produced -- it faults three frames later,
// in a different function, on a different thread, which is why this looks unrelated to
// the resource exhaustion that causes it.
//
// MEASURED, live, from the crashed process (PID 14224) rather than inferred:
//
//   list A count = 1, entry[0] = { 1c 00 00 00 04 00 00 00 | 00 00 00 00 | ff ff ff ff }
//                              =  obj 0x40000001c, delay 0  -- delay had run 3->2->1->0
//   lock 0x1444917c0 = 1        -- still held: the drain died inside its own critical
//                                  section and never released it
//   resource list: static 18889 + dynamic 13878 + 1 = 32768, exactly the 0x8000 gate
//   refcount[0] = 7267          -- handle 0 is the invalid sentinel, and AddDynamicResource
//                                  increments its refcount on every failed allocation
//
// 0x40000001c is not a pointer at all: it is two dwords, 0x1c and 4. Every one of the 11
// enqueue call sites passes `[obj+0x50]` of an object built by a resource creator, and
// only one of them (0x140968e04) bothers to NULL-check it. When the resource space is
// exhausted the creators hand back objects whose +0x50 was never populated.
//
// The guard therefore sits on the two enqueues: 5-byte-aligned entry detours whose only
// job is to refuse to queue something that is not a readable object. It rejects exactly
// what the drain cannot survive -- NULL, misaligned, non-user-space, or not committed
// readable memory through +0x68 (the highest byte either drain touches is +0x62).
//
// Dropping is the correct action, not a leak: a value that is not a pointer owns nothing.
// It is also the behaviour the game itself already has at 0x140968e04, which skips the
// enqueue when the field is NULL -- this extends that one site's check to all eleven.
//
// All 11 callers ignore the return value (checked at every site; the apparent RAX reads
// are `nop dword ptr [rax]` padding), so returning without calling through is a no-op.
static const BYTE SIG_RETIRE_A[] = {
    0x48,0x89,0x5C,0x24,0x08, 0x57, 0x48,0x83,0xEC,0x60, 0x48,0x8B,0xD9,
    0x33,0xC0, 0xB9,0x01,0x00,0x00,0x00, 0xF0,0x0F,0xB1,0x0D, 0x14,0x48,0xB2,0x03 };
static const BYTE SIG_RETIRE_B[] = {
    0x48,0x89,0x5C,0x24,0x08, 0x57, 0x48,0x83,0xEC,0x60, 0x48,0x8B,0xD9,
    0x33,0xC0, 0xB9,0x01,0x00,0x00,0x00, 0xF0,0x0F,0xB1,0x0D, 0x94,0x12,0xB2,0x03 };
// The two share a 24-byte prologue and are told apart by the spinlock's rip displacement
// in the final four bytes. Corroborate with the delay initialiser both must contain:
//   mov dword ptr [rax+rcx*8+8], 3
static const BYTE CHK_DELAY[]  = { 0xC7, 0x44, 0xC8, 0x08, 0x03, 0x00, 0x00, 0x00 };
static const int  CHK_DELAY_AT = 0x12B;

// The drains read fields up to +0x62; require the whole object header to be present.
static const SIZE_T RETIREE_SPAN = 0x68;

typedef void (*RetireFn)(void* obj);

// ---------------------------------- Fix E: the collection walker's NULL collection
//
// Where the 200x100 crash went after Fix D caught the queued garbage. Fix D held -- the
// 0x14096eeee fault did not recur and the log records the refusal -- and the next fault
// landed here, still with the resource list exhausted (7189 failed allocations):
//
//   0x140954490  mov   [rsp+8], rbx
//   0x140954495  push  rdi
//   0x140954496  sub   rsp, 0x20
//   0x14095449a  xor   ebx, ebx
//   0x14095449c  mov   rdi, rcx
//   0x14095449f  cmp   dword ptr [rcx + 0x38], ebx   <-- ACCESS VIOLATION
//
// The whole function is 0x30 bytes: walk [obj+0x38] elements, calling 0x140940490 on
// each. It faults on the very first instruction that touches the object, so the object
// pointer itself is bad -- the same shape as Fix D's unvalidated queue entries, and the
// same underlying cause.
//
// It has three call sites, each sourcing the pointer differently:
//
//   0x140209699   mov rcx, [rdi + 0x1e0]
//   0x14060fa92   mov rcx, [[r15+8] + 0x410]
//   0x140868212   mov rcx, rbp
//
// Guarding the three sites separately would be the non-terminating move. Guarding the
// *helper's entry* covers all three at once, and is exact: with no collection there is
// nothing to walk, so returning immediately is what the loop would have done anyway had
// the count been readable. All three callers ignore the return value.
//
// Located by a signature that is unique in .text and stops short of the internal
// `call rel32` so a rebuilt binary does not shift it, corroborated by the loop-back test
// `cmp ebx,[rdi+0x38]; jb` at +0x20.
static const BYTE SIG_WALK[] = {
    0x48,0x89,0x5C,0x24,0x08, 0x57, 0x48,0x83,0xEC,0x20,
    0x33,0xDB, 0x48,0x8B,0xF9, 0x39,0x59,0x38, 0x76,0x11 };
static const BYTE CHK_WALK[]  = { 0x3B, 0x5F, 0x38, 0x72, 0xEF };  // cmp ebx,[rdi+0x38]; jb
static const int  CHK_WALK_AT = 0x20;

// The walker reads only [obj+0x38], but it then hands each element to 0x140940490, so
// require the object header rather than just the count word.
static const SIZE_T WALK_SPAN = 0x40;

// ---------------------- Fix B (index13): widen the descriptor index from 12 to 13 bits
//
// This is the ROOT CAUSE of the cap above, finally addressed rather than contained.
//
// The chunk loader allocates a descriptor slot and writes the FULL 16-BIT index into
// the 14-byte record it is building (0x14061a37c). It then hands that record to one of
// two producers -- 0x140ac4124 and 0x140ac4304, byte-for-byte identical -- which pack it
// into the 10-byte render record the consumers actually read. So the 14-byte record and
// the 10-byte record were never rival theories about one array: one FEEDS the other, and
// that is why a 16-bit write and a 12-bit read both genuinely exist in this binary.
//
// The packing is where the index is truncated:
//
//   0x140ac418c   mov   eax, 0x1000
//   0x140ac419b   movzx edi, word ptr [rsp+0x20]      ; leftover stack scratch
//   0x140ac41a5   or    di, ax                        ; bit 12 forced to 1, always
//   ...
//   0x140ac41db   movzx ecx, word ptr [r14]           ; the full 16-bit index
//   0x140ac41df   mov   eax, 0xfff                    ; <-- the insert mask
//   0x140ac41e9   xor   cx, di
//   0x140ac41ec   and   cx, ax
//   0x140ac41f5   xor   di, cx                        ; di = (di & ~mask) | (idx & mask)
//   ...
//   0x140ac4210   and   ax, r10w                      ; r10 = 0x1FFF -- ALREADY 13 BITS
//   0x140ac4224   mov   word ptr [rsp+0x20], ax       ; word[0]
//   0x140ac423f   movsd qword ptr [rax+rcx*2], xmm0   ; stored, stride *10
//
// Two facts fall out of that, and together they are the whole fix:
//
//   * Bit 12 is NOT DATA. It is the literal constant 0x1000 OR'd in at 0x140ac41a5 and
//     never read back by anything. An earlier measurement found it set in 417,293 of
//     419,327 records and concluded it carried asset-derived data; it does not -- the
//     ~2034 clear ones are simply cells no producer ever wrote.
//   * The final `and ax, 0x1FFF` already passes 13 bits. The ONLY thing capping the
//     index at 12 is the insert mask, so widening that mask to 0x1FFF makes the insert
//     overwrite bit 12 with index bit 12 and leaves `or di, 0x1000` dead.
//
// Six sites constrain the field: those two insert masks and four consumer masks. That
// list is exhaustive, not a sample. A scan for the *10 stride (`lea rX,[rY+rY*4]` then
// scale 2, or a further lea/add into a pointer) across all of .text finds 49 packed-
// record accesses in 15 functions; of those, word[0] is read at exactly four masked
// sites, written by exactly the two producers, and otherwise touched only by
// `shr ax,0xd` at 0x140ac27ca (bits 13-15, unaffected) and an opaque `movsd` record
// copy. THERE IS NO UNMASKED READ OF word[0] ANYWHERE. .pdata gaps were covered too:
// an exhaustive restart-at-every-byte sweep of 0x140ac2400..0x140ac4600 -- which defeats
// the jump table at 0x140ac36e0 that stops naive linear disassembly -- finds the same
// six and nothing else.
//
// Result: 8192 addressable slots. Measured demand on a fully revealed ludicrous map was
// 4096 + 1959 = ~6055, so the sink should stop firing entirely. The cap STAYS, because
// 13 bits is 8192 and not unlimited; it just moves to 8192.
//
// It also repairs a second defect for free. The producers increment the refcount with
// the UNMASKED 16-bit index (0x140ac4189, 0x140ac4359) while the consumers decrement
// with the masked one, so above 4096 those two disagree and lifetime bookkeeping rots
// independently of the aliasing crash. At 13 bits with the cap at 8192 they agree again.
//
// WHY THIS RUNS IN DllMain AND NOT ON THE WORKER
//
// Widening the mask changes how word[0] is INTERPRETED. Any packed record produced
// before the patch has bit 12 set to the old constant, so a consumer patched afterwards
// would read it as index+4096 -- which is precisely the failure mode of the earlier
// naive experiment that widened only the consumers. The patch must therefore land
// before the game can produce a single record. It needs no allocation, no CRT and no
// file I/O, so it is safe under the loader lock; the outcome is recorded here and the
// worker reports it once there is a log to write to.
#include "Index13Table.h"

static struct {
    int  ran;          // Apply() was reached at all
    int  applied;      // sites successfully written
    int  mismatched;   // sites whose context bytes do not match this build
    int  failed;       // VirtualProtect failures
    int  disabled;     // turned off by CivFixIndex13.txt
    uint32_t badRva;   // first mismatching site, for the report
} g_i13;

typedef void (*WalkFn)(void* obj);

typedef unsigned int (*AddDynFn)(void* list);
typedef unsigned int (*AllocFn)(void* grid, const void* desc);

// ---------------------------------------------------------------- state

static AllocFn g_orig  = nullptr;
static LONG    g_limit = DEFAULT_LIMIT;
// The grid destructor releases each descriptor's buffer with _aligned_free, so the sink
// buffer MUST come from the matching _aligned_malloc in the very same CRT instance.
// A VirtualAlloc block here faults inside ucrtbase -- that was a real crash.
typedef void* (__cdecl *AlignedMallocFn)(size_t, size_t);
static AlignedMallocFn g_alignedMalloc = nullptr;
static FILE*   g_log = nullptr;
static BYTE*   g_mod = nullptr;
static HMODULE g_self = nullptr;   // this DLL, for locating the CivFix*.txt toggles

static volatile LONG g_pass = 0, g_dup = 0, g_sinkHits = 0, g_lost = 0,
                     g_noSink = 0, g_recycled = 0, g_capped = 0;

// buf/tab are per grid: the game's destructor frees every descriptor's buffer exactly
// once, so each sink needs its own block, and the state must be rebuilt after teardown.
struct GridState { void* grid; volatile LONG sink; volatile LONG init; void* buf; void* tab; };
static GridState g_grids[8];
static CRITICAL_SECTION g_cs;

static AddDynFn g_origAddDyn = nullptr;
struct ListState { void* list; volatile LONG sink; volatile LONG init; };
static ListState g_lists[8];
static volatile LONG g_rlCalls = 0, g_rlExhaust = 0, g_rlSinkUsed = 0, g_rlNoSink = 0;

static RetireFn g_origRetireA = nullptr, g_origRetireB = nullptr;
static volatile LONG g_retA = 0, g_retB = 0, g_badA = 0, g_badB = 0;

static WalkFn g_origWalk = nullptr;
static volatile LONG g_walk = 0, g_badWalk = 0;

// .text bounds, so the AV report can tell a return address from a data pointer.
static BYTE*  g_text     = nullptr;
static size_t g_textSize = 0;
static LONG g_resSinkEnabled = 0;   // off: handle 0 is the game's own invalid sentinel

static void logf(const char* fmt, ...) {
    char buf[1024];
    va_list ap; va_start(ap, fmt);
    _vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, ap);
    va_end(ap);
    OutputDebugStringA(buf);
    fputs(buf, stdout); fflush(stdout);
    if (g_log) { fputs(buf, g_log); fflush(g_log); }
}

// Widen buffers its output because it runs under the loader lock, before any file
// can be opened. This drains it into the same log as everything else.
static void WidenSink(const char* s) { logf("%s", s); }

// ---------------------------------------------------------------- the game's spinlock
//
// Replicated from the prologue of Alloc: acquire when the word reads 0 by CAS to
// 0x80000000, release with a plain store of 0.

static bool LockTry(volatile LONG* p, int spins) {
    for (int i = 0; i < spins; ++i) {
        if (*p == 0 && _InterlockedCompareExchange(p, (LONG)0x80000000, 0) == 0)
            return true;
        _mm_pause();
    }
    return false;   // caller must cope; we never fake ownership
}

static void LockRel(volatile LONG* p) { _ReadWriteBarrier(); *p = 0; }

// ---------------------------------------------------------------- module helpers

static bool MainTextRange(BYTE** base, size_t* size, BYTE** mod) {
    BYTE* m = (BYTE*)GetModuleHandleW(nullptr);
    if (!m) return false;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)m;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    IMAGE_NT_HEADERS64* nt = (IMAGE_NT_HEADERS64*)(m + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
    IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i)
        if (memcmp(sec[i].Name, ".text", 5) == 0) {
            *base = m + sec[i].VirtualAddress;
            *size = sec[i].Misc.VirtualSize;
            *mod  = m;
            return true;
        }
    return false;
}

static BYTE* FindSig(BYTE* base, size_t size, const BYTE* sig, size_t n, int* count) {
    BYTE* first = nullptr; int c = 0;
    for (size_t i = 0; i + n <= size; ++i)
        if (memcmp(base + i, sig, n) == 0) { if (!first) first = base + i; ++c; }
    *count = c;
    return first;
}

static void* AllocNear(BYTE* near_, SIZE_T bytes) {
    SYSTEM_INFO si; GetSystemInfo(&si);
    const uintptr_t gran = si.dwAllocationGranularity;
    uintptr_t origin = (uintptr_t)near_;
    for (uintptr_t d = gran; d < 0x60000000ULL; d += gran)
        for (int dir = 0; dir < 2; ++dir) {
            uintptr_t a = (dir ? origin + d : origin - d) & ~(uintptr_t)(gran - 1);
            void* p = VirtualAlloc((void*)a, bytes, MEM_COMMIT | MEM_RESERVE,
                                   PAGE_EXECUTE_READWRITE);
            if (p) return p;
        }
    return nullptr;
}

// -------------------------------------------------- the 12->13 bit index widening
//
// Runs from DllMain, so everything here is raw Win32: no CRT, no file streams, no
// logging. See the "Fix B (index13)" block above for why it cannot wait for the worker.

// Read a single digit from a file beside this DLL. An environment variable is the wrong
// control here for the reason recorded in Widen.cpp: Steam launches the game with the
// environment Steam itself inherited, so a changed variable does nothing until Steam is
// restarted -- which has already cost this project two runs that looked like fresh
// failures. Returns -1 when the file is absent or holds no digit.
static int ReadDigitFile(HMODULE self, const wchar_t* name) {
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(self, path, MAX_PATH);
    if (!n || n >= MAX_PATH) return -1;
    wchar_t* slash = nullptr;
    for (wchar_t* p = path; *p; ++p)
        if (*p == L'\\') slash = p;
    if (!slash) return -1;
    size_t room = MAX_PATH - (size_t)(slash + 1 - path), i = 0;
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

static void Index13Apply(HMODULE self) {
    using namespace Index13;
    g_i13.ran = 1;

    if (ReadDigitFile(self, L"CivFixIndex13.txt") == 0) { g_i13.disabled = 1; return; }

    BYTE* mod = (BYTE*)GetModuleHandleW(nullptr);
    if (!mod) { g_i13.mismatched = kNSites; return; }

    // Verify every site before writing any. A half-widened field set is strictly worse
    // than an unwidened one -- producers and consumers would disagree about the format
    // of records already on the heap -- so a different build must fail cleanly.
    for (int i = 0; i < kNSites; ++i) {
        const Site& s = kSites[i];
        if (memcmp(mod + s.rva - CTX_BEFORE, s.ctx, CTX_LEN) != 0) {
            if (!g_i13.mismatched) g_i13.badRva = s.rva;
            ++g_i13.mismatched;
        }
    }
    if (g_i13.mismatched) return;

    for (int i = 0; i < kNSites; ++i) {
        const Site& s = kSites[i];
        BYTE* at = mod + s.rva + s.field;
        DWORD old;
        if (!VirtualProtect(at, 4, PAGE_EXECUTE_READWRITE, &old)) { ++g_i13.failed; continue; }
        *(uint32_t*)at = NEW_MASK;
        VirtualProtect(at, 4, old, &old);
        ++g_i13.applied;
    }
    FlushInstructionCache(GetCurrentProcess(), mod, 0x1000000);
}

// Reported from the worker, once there is somewhere to write. Returns the number of
// descriptor slots the index field can now address.
static LONG Index13Report(void) {
    using namespace Index13;
    if (g_i13.disabled) {
        logf("[capfix] index13: disabled by CivFixIndex13.txt - the descriptor index "
             "stays 12 bits and the cap stays at %d\n", DEFAULT_LIMIT);
        return DEFAULT_LIMIT;
    }
    if (!g_i13.ran) {                       // cannot happen; reported rather than assumed
        logf("[capfix] index13: never ran - descriptor index stays 12 bits\n");
        return DEFAULT_LIMIT;
    }
    if (g_i13.mismatched) {
        if (!HostExe::IsDX12()) {
            char exe[MAX_PATH];
            HostExe::Name(exe, sizeof(exe));
            logf("[capfix] index13: %d of %d sites do not match - NOTHING APPLIED. "
                 "*** WRONG EXECUTABLE: this process is %s *** these patches target "
                 "CivilizationVI_DX12.exe; relaunch and choose DirectX 12. The cap "
                 "stays at %d.\n", g_i13.mismatched, kNSites, exe, DEFAULT_LIMIT);
        } else {
            logf("[capfix] index13: %d of %d sites do not match this build (first at "
                 "module+0x%X) - NOTHING APPLIED. The table was generated for a "
                 "different CivilizationVI_DX12.exe, so the game has most likely been "
                 "updated; the cap stays at %d.\n",
                 g_i13.mismatched, kNSites, g_i13.badRva, DEFAULT_LIMIT);
        }
        return DEFAULT_LIMIT;
    }
    if (g_i13.failed) {
        logf("[capfix] index13: *** %d of %d writes FAILED *** the image is now "
             "partially widened and this process is NOT safe to play. Close the game.\n",
             g_i13.failed, kNSites);
        return DEFAULT_LIMIT;
    }
    logf("[capfix] index13: %d of %d mask sites widened 0x%X -> 0x%X in DllMain, before "
         "the game could produce a record - the terrain descriptor index is now 13 bits "
         "(%d addressable slots, up from %d)\n",
         g_i13.applied, kNSites, OLD_MASK, NEW_MASK, WIDE_LIMIT, DEFAULT_LIMIT);
    return WIDE_LIMIT;
}

// ---------------------------------------------------------------- the hook

static GridState* StateFor(void* grid) {
    for (int i = 0; i < ARRAYSIZE(g_grids); ++i)
        if (g_grids[i].grid == grid) return &g_grids[i];
    EnterCriticalSection(&g_cs);
    GridState* st = nullptr;
    for (int i = 0; i < ARRAYSIZE(g_grids); ++i) {
        if (g_grids[i].grid == grid) { st = &g_grids[i]; break; }
        if (!g_grids[i].grid) { g_grids[i].grid = grid; g_grids[i].sink = -1;
                                st = &g_grids[i]; break; }
    }
    LeaveCriticalSection(&g_cs);
    return st;
}

// Claim one ordinary slot and point it at an oversized zero buffer. Done early, while
// the table still has room, so it costs one slot out of the limit and nothing else.
//
// The buffer is allocated with the game's own _aligned_malloc and then deliberately
// handed over: the grid destructor walks every slot and _aligned_free's the pointer at
// desc+8, so this block is released by the game exactly like any other. We must never
// free it ourselves, and never share one block between two grids.
static void MakeSink(BYTE* g, GridState* st) {
    if (!g_alignedMalloc) return;
    if (_InterlockedCompareExchange(&st->init, 1, 0) != 0) return;

    void* buf = g_alignedMalloc(SINK_BYTES, 16);
    if (!buf) { logf("[capfix] grid %p: sink buffer alloc failed\n", g); return; }
    memset(buf, 0, SINK_BYTES);

    BYTE desc[DESC_SIZE];
    memset(desc, 0, sizeof(desc));
    *(uint32_t*)(desc + 0) = 0xCAF1CAF1;      // nonzero: the free-scan must never
                                              // consider this slot reusable
    *(void**)(desc + 8) = buf;

    unsigned int idx = g_orig(g, desc);
    if (idx >= (unsigned int)g_limit) {
        logf("[capfix] sink allocation returned %u (>= limit) - unusable\n", idx);
        return;                                // the game owns `buf` now: do not free
    }
    // Pin the refcount so the game's sweeper can never free and recycle the slot.
    DWORD* refs = *(DWORD**)(g + OFF_REF);
    if (refs) refs[idx] = SINK_PIN;

    st->buf  = buf;
    st->tab  = *(void**)(g + OFF_TAB);
    st->sink = (LONG)idx;
    logf("[capfix] grid %p: sink descriptor = slot %u, buffer %p (%llu KB, from the "
         "game's _aligned_malloc), refcount pinned\n",
         g, idx, buf, (unsigned long long)(SINK_BYTES / 1024));
}

static unsigned int HkAlloc(void* grid, const void* desc) {
    if (!grid || !desc || !g_orig) return g_orig ? g_orig(grid, desc) : 0;

    BYTE* g = (BYTE*)grid;
    GridState* st = StateFor(grid);

    // The grid can be torn down and reused (returning to the main menu does exactly
    // this): the destructor frees every descriptor buffer -- ours included -- and resets
    // the count. Detect that and rebuild, or we would hand out a slot index into a table
    // that no longer holds our sink, backed by memory the game has already freed.
    if (st && st->init) {
        LONG cnt = *(volatile LONG*)(g + OFF_COUNT);
        if (cnt <= st->sink || *(void**)(g + OFF_TAB) != st->tab) {
            EnterCriticalSection(&g_cs);
            if (st->init) {
                logf("[capfix] grid %p: table reset (count=%ld, sink was slot %ld) - "
                     "the game freed our sink buffer with it; rebuilding\n",
                     g, cnt, st->sink);
                st->sink = -1; st->buf = nullptr; st->tab = nullptr;
                _InterlockedExchange(&st->init, 0);
            }
            LeaveCriticalSection(&g_cs);
        }
    }
    if (st && !st->init && g_alignedMalloc) MakeSink(g, st);

    LONG count = *(volatile LONG*)(g + OFF_COUNT);
    if (count < g_limit) {                       // the overwhelmingly common path:
        _InterlockedIncrement(&g_pass);          // untouched original behaviour
        return g_orig(grid, desc);
    }

    if (_InterlockedIncrement(&g_capped) == 1)
        logf("[capfix] *** CAP ENGAGED *** count reached %ld; the table would have grown "
             "past the 12-bit index here\n", count);

    volatile LONG* lock = (volatile LONG*)(g + OFF_LOCK);
    BYTE* tab = nullptr;
    unsigned int idx = (unsigned int)-1;

    // Dedupe under the game's own lock, so we never read the array mid-realloc.
    if (LockTry(lock, 200000)) {
        LONG cnt = *(volatile LONG*)(g + OFF_COUNT);
        if (cnt < g_limit) {                     // lost a race; room appeared
            LockRel(lock);
            _InterlockedIncrement(&g_pass);
            return g_orig(grid, desc);
        }
        tab = *(BYTE**)(g + OFF_TAB);
        if (tab) {
            LONG n = cnt < g_limit ? cnt : g_limit;
            for (LONG i = 0; i < n; ++i)
                if (memcmp(tab + (size_t)i * DESC_SIZE, desc, DESC_SIZE) == 0) {
                    idx = (unsigned int)i;
                    break;
                }
            // A looser rule -- alias on the BUFFER POINTER alone, since every render
            // consumer reads only desc.ptr and the leading u32 is just a liveness
            // marker -- was measured here and is DEAD. Across 1959 overflows on a
            // fully revealed ludicrous map, ptrdup was 0: not one overflow descriptor
            // shared a buffer with any live slot. Every descriptor owns its own
            // buffer, so no dedup at any strictness can help, and the per-entry
            // pointer compare that measured it has been removed.
        }
        LockRel(lock);
    } else {
        _InterlockedIncrement(&g_lost);          // fall through to the sink; safe
        tab = *(BYTE**)(g + OFF_TAB);
    }

    if (idx != (unsigned int)-1) { _InterlockedIncrement(&g_dup); return idx; }

    if (st && st->sink >= 0) {
        // Diagnostic only: the pinned refcount should make this impossible.
        if (tab && *(void**)(tab + (size_t)st->sink * DESC_SIZE + 8) != st->buf)
            if (_InterlockedIncrement(&g_recycled) == 1)
                logf("[capfix] WARNING: sink slot %ld was recycled by the game\n", st->sink);
        _InterlockedIncrement(&g_sinkHits);
        return (unsigned int)st->sink;
    }

    // No sink. Returning slot 0 was the old fallback and it is NOT safe: B is 13 bits
    // over 10-byte records, so a record aimed at the sink's 128 KB will read far past
    // slot 0's real buffer. That corrupts the heap, and heap corruption fail-fasts --
    // process gone, no exception, zero-byte dump. Observed exactly that.
    //
    // Passing through to the game instead restores the original 12-bit truncation bug.
    // That is a clean crash rather than silent corruption, and it is honest: without a
    // sink this fix cannot hold, so it stops pretending to.
    if (_InterlockedIncrement(&g_noSink) == 1)
        logf("[capfix] ERROR: no sink for grid %p - the cap cannot hold safely, passing "
             "through to the game. The original overflow behaviour is back; fix the sink "
             "allocator rather than playing on.\n", g);
    return g_orig(grid, desc);
}

// ---------------------------------------------------------------- crash reporting
//
// The first-chance handler below turns a user's crash into something diagnosable
// without a debugger: the faulting RVA, the registers, the caller chain, and every
// counter this module keeps. That last part matters most -- a fault while
// `exhausted` or `capped` is nonzero is a different bug from the same fault while
// both are 0.

static volatile LONG g_avLogged = 0;

// Everything this module knows about the run so far.
static void DumpCounters(void) {
    logf("[capfix]     desc pass=%ld capped=%ld dup=%ld sink=%ld nosink=%ld\n",
         g_pass, g_capped, g_dup, g_sinkHits, g_noSink);
    logf("[capfix]     res  calls=%ld exhausted=%ld nosink=%ld  (handle limit %d)\n",
         g_rlCalls, g_rlExhaust, g_rlNoSink, Widen::Limit());
    logf("[capfix]     retire A queued=%ld refused=%ld  B queued=%ld refused=%ld  "
         "walk called=%ld skipped=%ld\n",
         g_retA, g_badA, g_retB, g_badB, g_walk, g_badWalk);
}

// Is this address inside the game's own .text? Then it is code, and on the stack it is
// almost certainly a return address.
static bool InText(uintptr_t v) {
    return g_text && v >= (uintptr_t)g_text && v < (uintptr_t)g_text + g_textSize;
}

// Readable without faulting? Used to walk the stack safely from inside the handler.
static bool Readable(const void* p, SIZE_T n) {
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(p, &mbi, sizeof(mbi)) != sizeof(mbi)) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;
    const DWORD readable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                           PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                           PAGE_EXECUTE_WRITECOPY;
    if (!(mbi.Protect & readable)) return false;
    return (uintptr_t)p + n <= (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
}

// Everything a minidump would have given us, written at first chance while the registers
// are still the faulting ones. The point is the return-address scan: it names the caller
// of whatever faulted, which is the one thing static analysis cannot pin down when a
// helper has several call sites.
static void DumpFaultContext(EXCEPTION_POINTERS* ep) {
    const EXCEPTION_RECORD* er = ep->ExceptionRecord;
    const CONTEXT* c = ep->ContextRecord;

    const char* what = "access violation";
    if (er->NumberParameters >= 2) {
        switch (er->ExceptionInformation[0]) {
            case 0:  what = "READING";  break;
            case 1:  what = "WRITING";  break;
            case 8:  what = "EXECUTING"; break;
            default: break;
        }
    }
    logf("[capfix] === FAULT: %s %p at RIP %p (module+0x%llX) ===\n",
         what, er->NumberParameters >= 2 ? (void*)er->ExceptionInformation[1] : nullptr,
         (void*)c->Rip, (unsigned long long)((BYTE*)c->Rip - g_mod));

    static const char* kNames[16] = { "RAX","RCX","RDX","RBX","RSP","RBP","RSI","RDI",
                                      "R8","R9","R10","R11","R12","R13","R14","R15" };
    const DWORD64 regs[16] = { c->Rax,c->Rcx,c->Rdx,c->Rbx,c->Rsp,c->Rbp,c->Rsi,c->Rdi,
                               c->R8,c->R9,c->R10,c->R11,c->R12,c->R13,c->R14,c->R15 };
    for (int i = 0; i < 16; i += 2) {
        char ann[2][64] = { { 0 }, { 0 } };
        for (int k = 0; k < 2; ++k) {
            uintptr_t v = (uintptr_t)regs[i + k];
            if (g_mod && v >= (uintptr_t)g_mod && v < (uintptr_t)g_mod + 0x10000000)
                sprintf_s(ann[k], " module+0x%llX%s",
                          (unsigned long long)(v - (uintptr_t)g_mod),
                          InText(v) ? " (.text)" : "");
            else if (v && v < 0x10000)
                sprintf_s(ann[k], " <near-NULL>");
        }
        logf("[capfix]   %-3s %016llX%-28s  %-3s %016llX%s\n",
             kNames[i], (unsigned long long)regs[i], ann[0],
             kNames[i + 1], (unsigned long long)regs[i + 1], ann[1]);
    }

    // Return-address scan: every qword above RSP that lands in .text.
    logf("[capfix]   --- stack, addresses into .text (the caller chain) ---\n");
    DWORD64* sp = (DWORD64*)c->Rsp;
    int shown = 0;
    for (int i = 0; i < 256 && shown < 24; ++i) {
        if (!Readable(sp + i, sizeof(DWORD64))) break;
        uintptr_t v = (uintptr_t)sp[i];
        if (!InText(v)) continue;
        logf("[capfix]     [rsp+0x%03X] %016llX  module+0x%llX\n",
             i * 8, (unsigned long long)v,
             (unsigned long long)(v - (uintptr_t)g_mod));
        ++shown;
    }
    if (!shown) logf("[capfix]     (no .text addresses found above RSP)\n");
}

static LONG CALLBACK VehCapfix(EXCEPTION_POINTERS* ep) {
    // First-chance AV: the periodic poll can be up to a second stale and the process is
    // about to die, so capture state here, while the registers are still the faulting
    // ones. Then let the game handle it as usual so its own crash dump is still produced.
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
        InterlockedIncrement(&g_avLogged) <= 3) {
        DumpFaultContext(ep);
        DumpCounters();
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

// ---------------------------------------------------------------- resource-list hook

static ListState* ListStateFor(void* list) {
    for (int i = 0; i < ARRAYSIZE(g_lists); ++i)
        if (g_lists[i].list == list) return &g_lists[i];
    EnterCriticalSection(&g_cs);
    ListState* st = nullptr;
    for (int i = 0; i < ARRAYSIZE(g_lists); ++i) {
        if (g_lists[i].list == list) { st = &g_lists[i]; break; }
        if (!g_lists[i].list) { g_lists[i].list = list; g_lists[i].sink = -1;
                                st = &g_lists[i]; break; }
    }
    LeaveCriticalSection(&g_cs);
    return st;
}

static unsigned int HkAddDyn(void* list) {
    if (!list || !g_origAddDyn) return g_origAddDyn ? g_origAddDyn(list) : 0;

    ListState* st = ListStateFor(list);

    // Reserve one handle the first time we see this list, while the space is empty.
    // It is never released, so it stays valid for the life of the process.
    if (g_resSinkEnabled && st && _InterlockedCompareExchange(&st->init, 1, 0) == 0) {
        unsigned int s = g_origAddDyn(list);
        if (s != 0) {
            st->sink = (LONG)s;
            logf("[capfix] list %p: reserved sink handle %u (static=%d dynamic=%d)\n",
                 list, s, *(int*)((BYTE*)list + OFF_RL_STATIC),
                 *(int*)((BYTE*)list + OFF_RL_DYNAMIC));
        } else {
            logf("[capfix] list %p: could not reserve a sink handle - already exhausted\n",
                 list);
        }
    }

    unsigned int h = g_origAddDyn(list);
    _InterlockedIncrement(&g_rlCalls);
    if (h != 0) return h;                       // normal: untouched behaviour

    if (_InterlockedIncrement(&g_rlExhaust) == 1)
        logf("[capfix] *** RESOURCE LIST EXHAUSTED *** static=%d dynamic=%d free=%d "
             "(limit %d) - returning the game's invalid-handle sentinel 0\n",
             *(int*)((BYTE*)list + OFF_RL_STATIC),
             *(int*)((BYTE*)list + OFF_RL_DYNAMIC),
             *(int*)((BYTE*)list + OFF_RL_FREECNT), Widen::Limit());

    if (g_resSinkEnabled && st && st->sink > 0) {
        _InterlockedIncrement(&g_rlSinkUsed);
        return (unsigned int)st->sink;
    }
    _InterlockedIncrement(&g_rlNoSink);
    return 0;                                   // 0 = invalid handle, as the game intends
}

// ------------------------------------------------- deferred-destroy queue guards

// True only for something that can actually be dereferenced as an object of `span` bytes.
// Deliberately structural: it rejects values that cannot be objects, and never
// second-guesses a real pointer.
static bool ObjectReadable(void* p, SIZE_T span) {
    const uintptr_t v = (uintptr_t)p;
    if (!v) return false;                       // the game skips this itself at 0x140968e04
    if (v & 7) return false;                    // 0x40000001c failed here: & 7 == 4
    if (v < 0x10000) return false;              // null page
    if (v >= 0x7FFFFFFF0000ULL) return false;   // above user address space

    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(p, &mbi, sizeof(mbi)) != sizeof(mbi)) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;
    const DWORD readable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                           PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                           PAGE_EXECUTE_WRITECOPY;
    if (!(mbi.Protect & readable)) return false;

    // The whole header being read must sit inside this one committed region.
    const uintptr_t end = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
    return v + span <= end;
}

static bool RetireePlausible(void* p) { return ObjectReadable(p, RETIREE_SPAN); }

static void RetireReject(void* obj, void* ra, const char* which, volatile LONG* n) {
    LONG k = _InterlockedIncrement(n);
    if (k <= 8 || (k % 512) == 0)
        logf("[capfix] retire %s: REFUSED to queue %p from module+0x%llX "
             "(#%ld; res exhausted=%ld) - the drain would have faulted on it\n",
             which, obj, (unsigned long long)((BYTE*)ra - g_mod), k, g_rlExhaust);
}

__declspec(noinline) static void HkRetireA(void* obj) {
    _InterlockedIncrement(&g_retA);
    if (!RetireePlausible(obj)) {
        RetireReject(obj, _ReturnAddress(), "A", &g_badA);
        return;
    }
    if (g_origRetireA) g_origRetireA(obj);
}

__declspec(noinline) static void HkRetireB(void* obj) {
    _InterlockedIncrement(&g_retB);
    if (!RetireePlausible(obj)) {
        RetireReject(obj, _ReturnAddress(), "B", &g_badB);
        return;
    }
    if (g_origRetireB) g_origRetireB(obj);
}

__declspec(noinline) static void HkWalk(void* obj) {
    _InterlockedIncrement(&g_walk);
    if (!ObjectReadable(obj, WALK_SPAN)) {
        LONG k = _InterlockedIncrement(&g_badWalk);
        if (k <= 8 || (k % 512) == 0)
            logf("[capfix] walk: SKIPPED collection %p from module+0x%llX "
                 "(#%ld; res exhausted=%ld) - [obj+0x38] was not readable\n",
                 obj, (unsigned long long)((BYTE*)_ReturnAddress() - g_mod),
                 k, g_rlExhaust);
        return;
    }
    if (g_origWalk) g_origWalk(obj);
}

// ---------------------------------------------------------------- install

static bool Detour(BYTE* target, void* hook, void** origOut, const char* what) {
    BYTE* page = (BYTE*)AllocNear(target, 0x1000);
    if (!page) { logf("[capfix] no stub page within +-2GB of %p\n", target); return false; }

    // near thunk: jmp [rip+0] ; qword hook   (the DLL is far outside rel32 range)
    BYTE* thunk = page;
    thunk[0] = 0xFF; thunk[1] = 0x25; *(int32_t*)(thunk + 2) = 0;
    *(uint64_t*)(thunk + 6) = (uint64_t)hook;

    // trampoline: stolen bytes ; jmp [rip+0] ; qword target+STOLEN
    BYTE* tramp = page + 0x40;
    memcpy(tramp, target, STOLEN);
    tramp[STOLEN] = 0xFF; tramp[STOLEN + 1] = 0x25;
    *(int32_t*)(tramp + STOLEN + 2) = 0;
    *(uint64_t*)(tramp + STOLEN + 6) = (uint64_t)(target + STOLEN);

    intptr_t rel = (intptr_t)thunk - (intptr_t)(target + 5);
    if (rel < INT32_MIN || rel > INT32_MAX) {
        logf("[capfix] %s: thunk %p out of rel32 range of %p - aborting\n",
             what, thunk, target);
        VirtualFree(page, 0, MEM_RELEASE); return false;
    }

    DWORD old;
    if (!VirtualProtect(target, 5, PAGE_EXECUTE_READWRITE, &old)) {
        logf("[capfix] %s: VirtualProtect failed %lu\n", what, GetLastError());
        VirtualFree(page, 0, MEM_RELEASE); return false;
    }
    *origOut = tramp;                 // publish before the target can reach the hook
    target[0] = 0xE9; *(int32_t*)(target + 1) = (int32_t)rel;
    VirtualProtect(target, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), target, 5);
    FlushInstructionCache(GetCurrentProcess(), page, 0x1000);

    logf("[capfix] %s: detour installed at %p (module+0x%llX), trampoline %p\n",
         what, target, (unsigned long long)(target - g_mod), tramp);
    return true;
}

// Resolve the allocator whose blocks the grid destructor will release.
//
// The destructor releases each descriptor buffer through a thunk to _aligned_free, so
// the sink buffer must come from the matching _aligned_malloc in the same CRT. Reading
// that from the thunk's own bytes is fragile -- it is `48 FF 25` here, the REX-prefixed
// form, not the 6-byte `FF 25` -- so instead walk the module's import directory and take
// the pointers the loader already bound. That is encoding-independent, RVA-independent,
// and yields exactly the function the game itself will call.
static void* FindImport(BYTE* mod, const char* want) {
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)mod;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;
    IMAGE_NT_HEADERS64* nt = (IMAGE_NT_HEADERS64*)(mod + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return nullptr;

    const IMAGE_DATA_DIRECTORY& d =
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!d.VirtualAddress || !d.Size) return nullptr;

    for (IMAGE_IMPORT_DESCRIPTOR* imp = (IMAGE_IMPORT_DESCRIPTOR*)(mod + d.VirtualAddress);
         imp->Name; ++imp) {
        DWORD nameRva = imp->OriginalFirstThunk ? imp->OriginalFirstThunk : imp->FirstThunk;
        if (!nameRva || !imp->FirstThunk) continue;
        IMAGE_THUNK_DATA64* names = (IMAGE_THUNK_DATA64*)(mod + nameRva);
        IMAGE_THUNK_DATA64* iat   = (IMAGE_THUNK_DATA64*)(mod + imp->FirstThunk);
        for (int i = 0; names[i].u1.AddressOfData; ++i) {
            if (IMAGE_SNAP_BY_ORDINAL64(names[i].u1.Ordinal)) continue;
            IMAGE_IMPORT_BY_NAME* n =
                (IMAGE_IMPORT_BY_NAME*)(mod + names[i].u1.AddressOfData);
            if (strcmp((const char*)n->Name, want) == 0)
                return (void*)iat[i].u1.Function;
        }
    }
    return nullptr;
}

static bool ResolveGameHeap(BYTE* mod) {
    void* am = FindImport(mod, "_aligned_malloc");
    void* af = FindImport(mod, "_aligned_free");
    if (!am || !af) {
        logf("[capfix] heap: _aligned_malloc=%p _aligned_free=%p not both found in the "
             "import table - SINK DISABLED\n", am, af);
        return false;
    }
    g_alignedMalloc = (AlignedMallocFn)am;

    HMODULE crt = GetModuleHandleW(L"ucrtbase.dll");
    void* crtFree = crt ? (void*)GetProcAddress(crt, "_aligned_free") : nullptr;
    logf("[capfix] heap: sink buffers come from the game's own imported _aligned_malloc "
         "%p; the destructor frees through _aligned_free %p (ucrtbase's is %p, %s)\n",
         am, af, crtFree, (crtFree && crtFree == af) ? "same" : "differs - using the game's");
    return true;
}

// Find a signature, corroborate a second byte pattern at a fixed offset, and detour it.
static bool FindAndDetour(BYTE* text, size_t tsize, const BYTE* sig, size_t siglen,
                          const BYTE* chk, size_t chklen, int chkAt,
                          void* hook, void** origOut, const char* what) {
    int n = 0;
    BYTE* fn = FindSig(text, tsize, sig, siglen, &n);
    if (!fn || n != 1) {
        logf("[capfix] %s: signature %s (%d matches) - NOT HOOKED\n",
             what, fn ? "ambiguous" : "NOT FOUND", n);
        return false;
    }
    if (memcmp(fn + chkAt, chk, chklen) != 0) {
        logf("[capfix] %s: found %p but the check bytes at +0x%X do not match "
             "- wrong function or different layout, NOT HOOKED\n", what, fn, chkAt);
        return false;
    }
    logf("[capfix] %s: located at %p (module+0x%llX), check bytes verified at +0x%X\n",
         what, fn, (unsigned long long)(fn - g_mod), chkAt);
    return Detour(fn, hook, origOut, what);
}

static DWORD WINAPI Worker(LPVOID) {
    // Which executable this is. Named up front because if it is the wrong one, every
    // "does not match" line below follows from that single fact and nothing else in the
    // log is worth reading.
    {
        char exe[MAX_PATH];
        HostExe::Name(exe, sizeof(exe));
        if (HostExe::IsDX12()) {
            logf("[capfix] host: %s\n", exe);
        } else {
            logf("[capfix] *** WRONG EXECUTABLE *** host is %s. Civ VI ships two "
                 "executables with different code, and everything here targets "
                 "CivilizationVI_DX12.exe. Nothing below will apply. Quit, relaunch, "
                 "and choose DirectX 12 at the launcher.\n", exe);
        }
    }

    // What the index field can address now, which depends on whether the widening in
    // DllMain actually applied. This is reported before anything else because every
    // other number in this log is downstream of it.
    g_limit = Index13Report();

    // An explicit request still wins, but it is now bounded by the field: a limit above
    // what the index can address is the original bug, so it is refused rather than
    // honoured. That is the whole point of capping in the first place.
    char* e = nullptr; size_t elen = 0;
    if (_dupenv_s(&e, &elen, "CIVFIX_DESC_LIMIT") == 0 && e) {
        long v = strtol(e, nullptr, 0);
        if (v >= 64 && v <= g_limit) g_limit = (LONG)v;
        else logf("[capfix] CIVFIX_DESC_LIMIT=%ld ignored - outside 64..%ld, which is "
                  "what the index field can address on this run\n", v, g_limit);
        free(e);
    }
    logf("[capfix] descriptor limit = %ld\n", g_limit);

    char* r = nullptr; size_t rlen = 0;
    if (_dupenv_s(&r, &rlen, "CIVFIX_RES_SINK") == 0 && r) {
        g_resSinkEnabled = (strtol(r, nullptr, 0) != 0);
        free(r);
    }
    logf("[capfix] resource sink = %s (handle 0 is the game's invalid sentinel; the "
         "sink is known to cause crashes and is off unless explicitly requested)\n",
         g_resSinkEnabled ? "ON" : "off (observe only)");

    BYTE *text, *mod; size_t tsize;
    for (int i = 0; i < 120 && !MainTextRange(&text, &tsize, &mod); ++i) Sleep(500);
    if (!MainTextRange(&text, &tsize, &mod)) { logf("[capfix] no .text\n"); return 0; }
    g_mod = mod; g_text = text; g_textSize = tsize;
    logf("[capfix] module %p  .text %p size %llu\n", mod, text, (unsigned long long)tsize);

    ResolveGameHeap(mod);   // sets g_alignedMalloc, or leaves the sink disabled

    // Limit 1: the 12-bit terrain descriptor index.
    bool one = FindAndDetour(text, tsize, SIG_ALLOC, sizeof(SIG_ALLOC),
                             CHK_INC, sizeof(CHK_INC), CHK_INC_AT,
                             (void*)&HkAlloc, (void**)&g_orig, "descriptor allocator");
    // Limit 2: the 15-bit renderer resource handle. Independent -- one may install
    // without the other, and each is useful alone.
    bool two = FindAndDetour(text, tsize, SIG_ADDDYN, sizeof(SIG_ADDDYN),
                             CHK_CAP, sizeof(CHK_CAP), CHK_CAP_AT,
                             (void*)&HkAddDyn, (void**)&g_origAddDyn, "AddDynamicResource");
    // Limit 3: the two deferred-destroy queues, which dereference whatever pointer was
    // handed to them three frames earlier without ever checking it.
    bool guards = false;
    char* q = nullptr; size_t qlen = 0;
    bool skipRetire = (_dupenv_s(&q, &qlen, "CIVFIX_NO_RETIREGUARD") == 0 && q &&
                       strtol(q, nullptr, 0) != 0);
    if (q) free(q);
    if (skipRetire) {
        logf("[capfix] retire guards: disabled by CIVFIX_NO_RETIREGUARD\n");
    } else {
        bool a = FindAndDetour(text, tsize, SIG_RETIRE_A, sizeof(SIG_RETIRE_A),
                               CHK_DELAY, sizeof(CHK_DELAY), CHK_DELAY_AT,
                               (void*)&HkRetireA, (void**)&g_origRetireA,
                               "retire queue A (0x144491600)");
        bool bq = FindAndDetour(text, tsize, SIG_RETIRE_B, sizeof(SIG_RETIRE_B),
                                CHK_DELAY, sizeof(CHK_DELAY), CHK_DELAY_AT,
                                (void*)&HkRetireB, (void**)&g_origRetireB,
                                "retire queue B (0x144491618)");
        guards = a || bq;
        logf("[capfix] retire guards: %d of 2 installed - an object queued for deferred "
             "destruction is now validated before the drain can dereference it\n",
             (int)a + (int)bq);

        // Limit 4: the collection walker, which reads [obj+0x38] before checking anything.
        // Shares the retire guards' switch: same failure class, same cause.
        guards |= FindAndDetour(text, tsize, SIG_WALK, sizeof(SIG_WALK),
                              CHK_WALK, sizeof(CHK_WALK), CHK_WALK_AT,
                              (void*)&HkWalk, (void**)&g_origWalk,
                              "collection walker (0x140954490)");
    }

    if (!one && !two && !guards) {
        logf("[capfix] nothing applied - NO CHANGES MADE\n"); return 0;
    }

    // A first-chance access-violation handler, so that if the game does still crash the
    // log names the faulting RVA and the caller chain instead of leaving a bare dump.
    AddVectoredExceptionHandler(1, VehCapfix);

    LONG lastCap = -1, lastExh = -1, lastBadA = -1, lastBadB = -1, lastBadWalk = -1;
    for (;;) {
        Sleep(100);

        // Re-arms the old-object guard after a miss lifted it, checks the poisoned
        // header window, and drains anything Widen logged from a fault handler.
        Widen::Poll(WidenSink);

        if (g_badA != lastBadA || g_badB != lastBadB || g_badWalk != lastBadWalk) {
            lastBadA = g_badA; lastBadB = g_badB; lastBadWalk = g_badWalk;
            logf("[capfix] retire: A queued=%ld refused=%ld   B queued=%ld refused=%ld   "
                 "walk called=%ld skipped=%ld\n",
                 g_retA, g_badA, g_retB, g_badB, g_walk, g_badWalk);
        }

        if (g_capped != lastCap || g_rlExhaust != lastExh) {
            lastCap = g_capped; lastExh = g_rlExhaust;
            logf("[capfix] desc: pass=%ld capped=%ld (dup=%ld sink=%ld "
                 "lostlock=%ld nosink=%ld recycled=%ld)\n",
                 g_pass, g_capped, g_dup, g_sinkHits, g_lost, g_noSink,
                 g_recycled);
            logf("[capfix] res:  calls=%ld exhausted=%ld sinkUsed=%ld nosink=%ld\n",
                 g_rlCalls, g_rlExhaust, g_rlSinkUsed, g_rlNoSink);
            for (int i = 0; i < ARRAYSIZE(g_lists) && g_lists[i].list; ++i) {
                BYTE* L = (BYTE*)g_lists[i].list;
                logf("[capfix]   list %p: static=%d dynamic=%d free=%d  used=%d/%d\n",
                     L, *(int*)(L + OFF_RL_STATIC), *(int*)(L + OFF_RL_DYNAMIC),
                     *(int*)(L + OFF_RL_FREECNT),
                     *(int*)(L + OFF_RL_STATIC) + *(int*)(L + OFF_RL_DYNAMIC),
                     Widen::Limit());
            }
        }
    }
}

// Entry point. Called from DllMain on DLL_PROCESS_ATTACH.
//
// Everything happens on a worker thread: the patches target CivilizationVI_DX12.exe's
// own .text, and this DLL is loaded before that module is necessarily ready, so Worker
// polls for it.
//
// No AllocConsole -- output goes to LargeMapFix.log beside this DLL, so nothing here
// steals a console from a host that might want one.

// Everything real happens here, off the loader lock. Install() only starts this thread.
static DWORD WINAPI Bootstrap(LPVOID) {
    InitializeCriticalSection(&g_cs);
    for (int i = 0; i < ARRAYSIZE(g_grids); ++i) g_grids[i].sink = -1;
    for (int i = 0; i < ARRAYSIZE(g_lists); ++i) g_lists[i].sink = -1;

    char p[MAX_PATH];
    if (GetModuleFileNameA(g_self, p, MAX_PATH)) {
        char* s = strrchr(p, '\\');
        if (s) strcpy_s(s + 1, MAX_PATH - (s + 1 - p), "LargeMapFix.log");
    } else strcpy_s(p, "LargeMapFix.log");
    g_log = _fsopen(p, "w", _SH_DENYWR);
    logf("[capfix] attached, logging to %s\n", p);

    // Widen ran back in DllMain, long before this file existed. Its record of what
    // it verified and applied comes out first, so the log reads in causal order.
    Widen::FlushLog(WidenSink);

    return Worker(nullptr);
}

namespace LargeMapFix {
    // Deliberately does almost nothing: when this DLL is a statically-imported proxy it
    // runs during process initialisation, under the loader lock. Opening files or
    // initialising a critical section there is asking for a deadlock, so the only call
    // made is CreateThread -- the new thread does not run until the lock is released.
    void Install(HMODULE self) {
        g_self = self;
        // The one exception to "almost nothing". Widening the descriptor index changes
        // how a record's word[0] is read, so it has to land before the game can write a
        // single record -- a worker thread would lose that race. It only reads a file,
        // memcmps 6 windows and writes 6 dwords: no CRT, no allocation, no file streams,
        // nothing that can re-enter the loader. The result is logged by the worker.
        Index13Apply(self);
        HANDLE t = CreateThread(nullptr, 0, Bootstrap, nullptr, 0, nullptr);
        if (t) CloseHandle(t);
    }
}
