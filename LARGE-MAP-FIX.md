# Large-map crash fixes — full diagnosis

Civilization VI crashes on maps larger than the vanilla ceiling. The cases that prompted
this were 180×94 and then 200×100, the largest the game offers; the folklore limit is
128×80.

Several distinct crash sites were traced. Two of them are genuine repairs of a hard limit;
the rest turned out to be **downstream of the first limit**, and only the cheapest are
kept, as backstops. This document is ordered causally: the root cause first, then what it
was breaking.

All addresses are for the Steam DX12 build with `.text` at `0x140001000`, size
`16400332`.

| | | stock | after |
| --- | --- | --- | --- |
| **Fix A** | renderer resource handles | 32768 | 65534 |
| **Fix B** | terrain descriptor index | 4096 (12 bits) | 8192 (13 bits) |
| Fix C | descriptor cap | — | backstop behind B |
| Fix D | deferred-destroy queue validation | — | guard |
| Fix E | collection walker NULL check | — | guard |

**Tested:** a large map played through with `exhausted=0` and `capped=0`, including a
**Gathering Storm rising sea level** event — which re-generates coastal terrain mid-game,
exercising the descriptor allocator and the renderer hardest — with no crash.

---

## Fix A — the 15-bit renderer resource handle

**This is the upstream cause of most of the rest.**

`PartitionedResourceList::AddDynamicResource` at `0x1409405e0` hands out `uint16` handles
from a space shared with `AddStaticResource`. Statics count **up** from 1, dynamics count
**down** from `0x8000`, and the gate tests the combined count:

```
0x140940642   cmp ecx, 0x8000        ; the gate
0x14094064d   mov edi, 0x8000        ; the dynamic origin
0x140940653   sub di, ax
```

Measured at saturation:

```
180x94 :  static=18761  dynamic=14006  free=0  used=32767/32768
200x100:  static=18889  dynamic=13878  free=0  used=32767/32768
```

Both map sizes saturate it completely. Dynamic demand is greedy — it consumes whatever
the static baseline leaves. The static baseline barely moves with map size but **does**
move with graphics settings, which is why lowering them used to produce a working run
(`29299/32768`, with headroom).

On exhaustion the allocator returns 0. That is **correct behaviour**: the constructor at
`0x140973ff0` ends with `mov qword ptr [rbx], 1`, so the static count starts at 1 and
handle 0 is never issued — it is the reserved invalid sentinel. Fixes D, E and F all
exist because unchecked consumers treat that sentinel as a live handle.

### Why the limit can be raised

`0x8000` is not the limit of the handle *format*. It is the meet-in-the-middle split
point of a `uint16` space, so the format already has room for 65534.

Every consumer zero-extends before doing arithmetic (`movzx ecx,di`, then 64-bit
`add rcx,rcx`); every scan loop compares bounds in 32 bits
(`movzx eax,bx ; cmp eax,[rsi] ; jb`), so `inc bx` cannot wrap; and nothing anywhere
masks the value. A 59-site audit of every 16-bit-destination arithmetic instruction in
the renderer band found no truncation that matters. Raising the origin and the gate to
`0xFFFF` therefore changes no data format.

> An earlier version of this project recorded that this ceiling "cannot be raised because
> the backing storage is inline in the object at fixed offsets". The first half is true
> and the second half does not follow: inline storage means the count is baked into the
> object's **layout**, not that the layout is fixed.

### What actually blocks it

The object is a static at `module+0x6db3840`, `0x6a1180` bytes (~6.9 MB), so there is no
allocation size to bump:

```
+0x00000  next-handle counters (static in the low dword, dynamic in the high)
+0x00080  refcounts    0x8000 x 4   = 0x20000
+0x20080  live count, then 3 x 0x428 substructures
+0x20d00  main array   0x8000 x 16  = 0x80000    (indexed scale 8, handle x2)
+0xa0d00  tail fields
+0xa1180  records      0x8000 x 192 = 0x600000   (index x192 via lea+shl 6)
+0x6a1180 end -- the next global starts here and is separately referenced
```

**Three** arrays scale, not two. The record array was originally read as "3 × 64-byte
frame buffers" from its access pattern, and that was wrong by four orders of magnitude:
it is `0x8000` entries of 192 bytes. The mistake is worth recording because of *how* it
happened — a runtime-indexed array only ever exposes its **base** displacement, so a
displacement scan can see where it starts and never how far it runs. Sizes come from the
bulk-zero calls, which state them outright. **Infer layout from initialisers, not from
access patterns.**

### The fix

The object is relocated to a doubled block allocated within ±2GB of the module, and all
**320** references to it are rewritten in place: 140 register displacements, 105
rip-relative references, 31 image-relative references, 39 size/limit/displacement
immediates, and 5 unwind fields. Every code edit is a 4-byte field inside an instruction
that keeps its length — a `disp32` stays a `disp32` whatever value it holds — so nothing
is relocated, no instruction is stolen, and no trampoline exists.

Six sites state one of the three array sizes for *this* object; seven more state the same
constants for sibling resource lists (`0x140971840` → `0x14673d700`, `0x1409742b0` whose
main array is at `+0x20380`, and others). Only the six are touched.

Beyond the object itself, four things outside it must move with the limit:

* `AddStaticResource` has its **own** gate on the same combined count (`0x141561`).
  Widening only the dynamic one let dynamics reach 65535 while statics stayed capped —
  which the game logged, then turned into a `CreatePlacedResource` failure and a NULL
  deref in its own rollback path.
* The handle-space top, replicated as `0x8000 - dynamic_count` in three more places.
* `NumDescriptors` for the four D3D12 descriptor heaps indexed by the raw handle, all
  created in `0x1409515ba`. The moment the origin moves to `0xFFFF` the first dynamic
  resource indexes past the end of a `0x8000`-descriptor heap and the driver faults.
* The stack frame of `0x14095FDE0`, which holds a 2-bits-per-handle bitmap as a **local**.
  Growing `0x2058` → `0x4058` doubles the bitmap and its memset in one edit, and
  everything at or above `0x2030` shifts by `+0x2000`. Its unwind records in `.xdata`
  move with it.

### How the sites were chosen, and why not by range

The obvious approach — rewrite every displacement in the renderer band that falls in the
object's range — is unsound, and provably so on this binary. There are ten byte-identical
refcount-release functions at `0x14094d4d0 + 0xd0` stride: one template instantiated over
ten **different** objects. Three of them touch `+0x20080` and `+0x20088` on objects at
`0x14673d680`, `0x146c1e440` and elsewhere. A range scan rewrites those and corrupts three
unrelated structures.

So each site is included only when the object pointer is **provably** in that operand,
traced from a `lea reg,[rip -> the object]` through register copies and `lea reg,[A+B]`
folding, with every function whose pointer arrives as a parameter resolved by checking all
of its callers. Of the ten release families exactly one — `0x14094dc20`, whose three
callers all pass our object — is in.

One more trap: the object legitimately appears in the **index** slot, because the
`+0xa1180` record accesses are `[rdi + r12 + 0xa118a]` with `r12` holding it. But only
ever with scale 1. Accepting a scaled index instead swept up
`lea rcx,[rax*8 + 0x675da00]` in `0x14096e100`, which is a different table entirely.
Index-slot matches therefore require scale 1.

### The guard page

The provenance trace is a linear scan; it does not model control flow, so a register
marked object-derived in one block is still marked in a block reached by a branch where it
holds something else. Both over- and under-inclusion are possible in principle, and a
**missed** site would read the old address and silently corrupt renderer memory. That is
the one failure mode worth engineering against, because it is invisible.

So after relocating, the old region is made `PAGE_NOACCESS`. Any site that still reaches
the old object faults immediately, and the handler names the exact offset and instruction
instead of leaving a corrupted frame to crash somewhere unrelated ten seconds later.

Note the gap this leaves, because it bit: a missed reference inside the unguardable head
sliver does **not** fault. The spinlock at `+0x40` lives there, and a CAS against poison
never succeeds, so the symptom is an infinite spin rather than a crash — and a spin writes
nothing, so the poison check cannot see it either. That is why the static side proves the
stronger property: after applying the plan, the generator re-scans **all** of `.text` and
asserts that **zero** references to the old object survive. Completeness is verified, not
hoped for; the guard is the backstop, not the primary control.

Neither end of the object is page-aligned, and both end pages are shared with neighbours
that are genuinely referenced — `0x146db3780..0x3830` below it, and the next global at
exactly `OBJ+0x6a1180` above it. So the guard covers only the wholly-owned pages (6.62 MB
of 6.9), and the two slivers either side are filled with `0xCD` and polled instead. Both
ends matter: rounding the upper bound *up* instead of down would protect the neighbour and
break it.

### Modes

`CivFixWiden.txt` beside the DLL:

| mode | effect |
| --- | --- |
| `0` / absent | off, nothing touched |
| `1` | relocate and guard, limit unchanged at 32768 |
| `2` | relocate, guard, and raise the limit to 65534 |

**Mode 1 is the whole point of having modes.** It separates "did I find every reference"
from "does a bigger limit work", so a failure in either has an unambiguous cause. In mode
1 the game should behave *exactly* as before — same handle count, same everything, just a
moved object — so any fault at all is a missed site.

---

## Fix B — the 12-bit terrain descriptor index

**The defect is a width mismatch, not a capacity problem.**

The grid owns a descriptor slot allocator at `0x140ac3ce0`. It spin-locks `[grid+0x80]`,
scans slots for a free one (`dword0 == 0`), and otherwise appends to the array at
`{+0x30 ptr, +0x38 cap, +0x40 count}`, 16 bytes per slot. The append site was pinned with
a hardware write watchpoint on the count field:

```
0x140ac3d88   inc dword ptr [rdi+0x40]
```

The allocator returns the slot index via `movzx eax, bx` — a **full 16-bit** value. Its
caller, the chunk loader, stores it 16-bit and unchecked:

```
0x14061a37c   mov word ptr [rdx], cx
```

But every consumer truncates to **12 bits**:

```
movzx edx, word [rax+rcx*2] ; and edx, 0x0FFF ; shl rdx,4 ; add rdx,[grid+0x30]
```

So at 4096 descriptors the index silently wraps: record 4096 reads descriptor 0. On a
180×94 map the count reaches 4167, and slots 4096..4166 become unreachable — index 4100
is read back as slot 4, whose buffer is a different size, and the vertex offset `B` then
runs off the end of it. That is the observed crash:

```
0x140ac34dc  movzx eax,[r9+rdx*2+4]   R9=0x137cf4f0c  RDX=0x8629 (=B*5)
fault address == R9 + 34345*2 + 4, exactly.
```

The refcount array at `grid+0x48` is corrupted the same way at `0x140ac40c1`.

### Where the truncation is

The chunk loader writes the full 16-bit index into the 14-byte record it builds, then
hands that record to one of two producers, `0x140ac4124` and `0x140ac4304`, which pack it
into the 10-byte record the renderer reads. The 14-byte and 10-byte records were never
rival readings of one array — **one feeds the other**, which is why a 16-bit write and a
12-bit read both genuinely exist. The truncation is in the packing:

```
0x140ac418c   mov   eax, 0x1000
0x140ac41a5   or    di, ax                     ; bit 12 forced to 1, unconditionally
0x140ac41db   movzx ecx, word ptr [r14]        ; the full 16-bit index
0x140ac41df   mov   eax, 0xfff                 ; <-- the insert mask
0x140ac41e9   xor   cx, di
0x140ac41ec   and   cx, ax
0x140ac41f5   xor   di, cx                     ; di = (di & ~mask) | (idx & mask)
0x140ac4210   and   ax, r10w                   ; r10 = 0x1FFF -- ALREADY 13 BITS
0x140ac4224   mov   word ptr [rsp+0x20], ax    ; word[0]
```

Two facts make this a six-immediate change:

* **Bit 12 is not data.** It is the literal `0x1000` OR'd in at `0x140ac41a5` and read
  back by nothing. An earlier measurement found it set in 417,293 of 419,327 records and
  concluded it held asset-derived flags; it does not — the ~2034 clear ones are cells no
  producer ever wrote.
* **The final `and ax, 0x1FFF` already passes 13 bits.** Only the insert mask caps the
  index at 12, so widening it makes the insert overwrite bit 12 with index bit 12 and
  leaves the `or di, 0x1000` dead.

| VA | instruction | role |
| --- | --- | --- |
| `0x140ac41df` | `mov eax, 0xfff` | producer A — insert mask |
| `0x140ac43b0` | `mov eax, 0xfff` | producer B — identical |
| `0x140ac265e` | `mov eax, 0xfff` | consumer — decref and free |
| `0x140ac2ec0` | `and edx, 0xfff` | consumer — render |
| `0x140ac3184` | `and eax, 0xfff` | consumer — render, the original crash site |
| `0x140ac40c1` | `and ecx, 0xfff` | consumer — refcount increment |

All six become `0x1FFF`. Every one is an `imm32`, so no instruction changes length and
nothing is relocated.

**The list is exhaustive, not a sample.** A scan for the `*10` stride across all of
`.text` finds 49 packed-record accesses in 15 functions; `word[0]` is read at exactly
those four masked sites, written by exactly those two producers, and otherwise touched
only by `shr ax,0xd` at `0x140ac27ca` (bits 13–15, a different field) and an opaque
`movsd` record copy. There is no unmasked read of `word[0]` anywhere. `.pdata` gaps were
covered too: an exhaustive restart-at-every-byte sweep of `0x140ac2400..0x140ac4600` —
which defeats the jump table at `0x140ac36e0` that stops naive linear disassembly — finds
the same six and nothing else.

### It also repairs the refcount mismatch

The producers increment the refcount with the **unmasked** 16-bit index (`0x140ac4189`,
`0x140ac4359`) while the consumers decrement with the masked one, so above 4096 the two
disagree and lifetime bookkeeping rots independently of the aliasing crash. At 13 bits
with the cap at 8192 they agree again.

### Why it runs in `DllMain`

Widening the mask changes how `word[0]` is *interpreted*. Any record produced beforehand
has bit 12 set to the old constant, so a consumer patched later would read it as
`index + 4096` — precisely the failure mode of an earlier experiment that widened only the
consumers. The patch must land before the game can write a single record, so a worker
thread would lose the race. It reads one file, `memcmp`s six context windows and writes six
dwords: no CRT, no allocation, nothing that can re-enter the loader.

### Verification

The generator that produces `src/Index13Table.h` proves that every occurrence of every
context window anywhere in `.text` is itself one of the six sites — a stronger property
than uniqueness, which the byte-identical producers make impossible without an absurd
window. A separate check applies the generated table to a copy of the image and asserts
that each site keeps its mnemonic and length, that no `0xfff` mask survives in the grid
module, and that the sites which must *not* change (`and ax,r10w`, `shr ax,0xd`, the two
`word[1]` B masks) are untouched.

Measured demand on a fully revealed ludicrous map was 4096 + 1959 ≈ **6055**, under 8192,
so `capped=0` is the expected result in the log.

---

## Fix C — the descriptor cap (backstop behind B)

13 bits is 8192, not unlimited, so the cap that predated Fix B still runs. It just sits at
8192 and is expected never to fire.

Below the cap the original allocator runs untouched. At the cap, the hook first tries to
**dedupe** — under the game's own spinlock, so the array is never read mid-realloc — and
returns the existing index if the 16-byte descriptor already exists. Otherwise it returns
a **sink slot**: one ordinary slot claimed at startup whose data pointer is a 128 KB
zero-filled buffer. The secondary index `B` is 13 bits over 10-byte records, so the highest
byte it can reach is `8191*10+6 = 81916`, inside 128 KB. Every possible `B` therefore reads
zeroes instead of running off another allocation. The sink's refcount is pinned to
`0x40000000` so the game's sweeper can never recycle it.

**Cost:** terrain that would have needed a slot past the cap renders blank instead of
crashing. Memory only — saves are untouched.

A looser dedupe rule — alias on the buffer pointer alone, since every render consumer reads
only `desc.ptr` — was measured and is **dead**: across 1959 overflows on a fully revealed
ludicrous map, not one overflow descriptor shared a buffer with any live slot. Every
descriptor owns its own buffer, so no dedupe at any strictness can help.

### The sink buffer must come from the game's allocator

This bit an earlier version, and the crash appeared far from its cause — on returning to
the main menu. The grid destructor at `0x140ac3fd0` walks every slot and releases each
descriptor's buffer:

```
0x140ac4019   cmp dword [rax+rcx*8], esi   ; slot empty?
0x140ac401c   je  0x140ac4028              ; yes -> skip
0x140ac401e   mov rcx, [rax+rcx*8+8]       ; desc.ptr
0x140ac4023   call 0x14098fea0             ; release it
```

`0x14098fea0` is a thunk to **`_aligned_free`**. The sink buffer had been allocated with
`VirtualAlloc`, which has no `_aligned_malloc` header behind it, so the free faulted every
time — inside `ucrtbase.dll`, with the sink buffer's address in `RCX`.

Three consequences, all handled:

* The buffer comes from **the game's own imported `_aligned_malloc`**, obtained by walking
  the module's import directory and taking the pointer the loader already bound. That is
  encoding-independent — the thunk is `48 FF 25` here, the REX-prefixed 7-byte form, not
  the 6-byte `FF 25` a hand-written byte check would expect.
* **Ownership transfers to the game.** The destructor frees it; this module never does.
* **One buffer per grid.** A single shared global would be a double free on the second
  teardown. State is rebuilt when the count resets or the table pointer changes, which is
  exactly what returning to the main menu does.

If the allocator cannot be resolved, the sink is disabled and the cap **passes through to
the game** rather than returning slot 0. Returning slot 0 was the old fallback and it is
not safe: `B` runs past slot 0's real buffer, corrupting the heap — which fail-fasts with
no exception and a zero-byte dump, far harder to diagnose than the crash it replaced.

---

## Fix D — the deferred-destroy queues' unvalidated object pointers

**This was the 200×100 crash.** The reported fault was
`EXCEPTION_ACCESS_VIOLATION: Error reading address 0x7d`, and this module's own handler
logged the faulting instruction:

```
0x14096eeee   movzx ecx, byte ptr [rbx + 0x61]     ; RBX = 0x40000001c
```

`0x1c + 0x61 = 0x7d`, exactly the reported address.

### The mechanism

`RBX` comes out of a **deferred-destroy queue**. There are two, identical in layout,
drained back to back by `0x14096e100`:

| queue | array ptr | count | enqueue |
| --- | --- | --- | --- |
| A | `0x144491600` | `0x144491610` | `0x14096cf90` |
| B | `0x144491618` | `0x144491628` | `0x140970590` |

Entries are 16 bytes, `{void* obj, uint32 delay, uint32}`. Both enqueues append with
`delay = 3` (`mov dword ptr [rax+rcx*8+8], 3`, at `+0x12b` in both). Each drain pass
decrements the delay and compacts the array; at zero the object is finally destroyed:

```
mov   ecx, [rbx + r8*8 + 8]    ; delay
test  ecx, ecx
jne   retain                   ; still in flight -> decrement, keep
mov   rbx, [rbx + r8*8]        ; delay == 0 -> destroy
movzx ecx, byte [rbx + 0x61]   ; queue A -- no NULL check, no validity check
mov   eax, [rbx + 0x54]        ; queue B at 0x14096ed0f -- likewise
```

So a bad pointer does not fault where it is produced. It faults **three frames later**, in
a different function, which is why the crash looks unrelated to what causes it.

### Measured, live, from the crashed process

Read out of the still-running crashed process rather than inferred:

```
queue A count = 1
entry[0] = 1c 00 00 00  04 00 00 00 | 00 00 00 00 | ff ff ff ff
         = obj 0x40000001c, delay 0        <- delay had already run 3 -> 2 -> 1 -> 0
lock 0x1444917c0 = 1                       <- still held: the drain died inside its own
                                              critical section and never released it
resource list: static 18889 + dynamic 13878 + 1 = 32768   <- exactly the 0x8000 gate
refcount[0] = 7267                         <- handle 0 is the invalid sentinel, and
                                              AddDynamicResource increments its refcount
                                              on every failed allocation
```

`0x40000001c` is not a pointer at all — it is two dwords, `0x1c` and `4`.

All **11** enqueue call sites pass `[obj+0x50]` of an object returned by a resource
creator, and exactly one of them (`0x140968e04`) NULL-checks it first. When the resource
handle space is exhausted the creators hand back objects whose `+0x50` was never
populated, and the other ten queue the garbage without looking.

### The fix

Entry detours on the two enqueues. Each validates the incoming pointer and refuses to queue
anything the drain could not survive: NULL, misaligned, outside user address space, or not
committed readable memory through `+0x68` (the highest byte either drain reads is `+0x62`).

Why it is narrow:

* The test is **structural** — it rejects values that cannot be objects and never
  second-guesses a real pointer. `0x40000001c` fails on `& 7 == 4` alone.
* Dropping is not a leak: a value that is not a pointer owns nothing.
* It is the behaviour **the game already has** at `0x140968e04`, which skips the enqueue
  when the field is NULL. This extends that one site's check to all eleven.
* All 11 callers ignore the return value (checked at every site — the apparent `RAX` reads
  are `nop dword ptr [rax]` padding), so returning without calling through is a no-op.
* Both are located by a signature that is **unique in `.text`** and corroborated by the
  delay initialiser at `+0x12b`; the 5 stolen bytes are one whole instruction
  (`mov [rsp+8], rbx`) with no rip-relative operand and no branch.

**Cost:** none in normal play — nothing valid is ever refused. Disable with
`CIVFIX_NO_RETIREGUARD=1`.

**Limitation, stated honestly:** the guard runs at *enqueue*, three frames before the
dereference. It catches a pointer that was already garbage when queued, which is what was
measured here. It would **not** catch an object that was valid at enqueue and freed before
the drain. If a crash recurs at `0x14096eeee` or `0x14096ed0f` with a plausible-looking
pointer, that is the remaining case and it needs a different fix.

With Fix A in place the producing condition (an exhausted handle space) should not occur
at all, so this guard is expected to sit idle — `refused=0`.

---

## Fix E — the collection walker's NULL collection

Where the 200×100 crash went **after** Fix D caught the queued garbage. Fix D held: the
`0x14096eeee` fault did not recur, and the log records the refusal with the producer named:

```
retire A: REFUSED to queue 000000040000001C from module+0x96AC47 (#1; res exhausted=7189)
retire: A queued=2 refused=1   B queued=65610 refused=0
```

`module+0x96AC47` is the return address of the `call` at **`0x14096ac42`**, in
`0x14096aaae` — one of the ten enqueue sites that does not NULL-check `[obj+0x50]`. Note
how rare the path is: queue A took only **2** enqueues all session and one of them was the
poisoned one, while queue B took 65610 and never produced a bad pointer.

The next fault was at `0x14095449F`, with the resource list still exhausted (7189 failed
allocations):

```
0x140954490  mov   [rsp+8], rbx
0x140954495  push  rdi
0x140954496  sub   rsp, 0x20
0x14095449a  xor   ebx, ebx
0x14095449c  mov   rdi, rcx
0x14095449f  cmp   dword ptr [rcx + 0x38], ebx    <-- ACCESS VIOLATION
```

The whole function is `0x30` bytes: walk `[obj+0x38]` elements, calling `0x140940490` on
each. It faults on the very first instruction that touches the object, so the object
pointer itself is bad — the same shape as Fixes D and F, and the same cause.

It has three call sites, each sourcing the pointer differently:

| site | pointer |
| --- | --- |
| `0x140209699` | `[rdi + 0x1e0]` |
| `0x14060fa92` | `[[r15+8] + 0x410]` |
| `0x140868212` | `rbp` |

Guarding the three separately would be the non-terminating move. Guarding **the helper's
entry** covers all three at once, and is exact: with no collection there is nothing to
walk, so returning immediately is what the loop would have done anyway had the count been
readable. All three callers ignore the return value.

Located by a signature unique in `.text` that stops short of the internal `call rel32`, so
a rebuilt binary does not shift it, corroborated by the loop-back test
`cmp ebx,[rdi+0x38]; jb` at `+0x20`. Shares `CIVFIX_NO_RETIREGUARD`.

---

## Configuration

Files beside the DLL, read at every start. A file rather than an environment variable
because Steam launches the game with the environment *Steam* inherited, so a changed
variable does nothing until Steam itself is restarted — which cost this project two runs
that looked like fresh failures.

| file | effect |
| --- | --- |
| `CivFixWiden.txt` | Fix A: `0`/absent = off, `1` = validation run, `2` = raise the limit |
| `CivFixIndex13.txt` | `0` disables Fix B (12-bit index, cap at 4096); absent = enabled |

Environment variables, all optional:

| variable | effect |
| --- | --- |
| `CIVFIX_DESC_LIMIT` | descriptor cap; defaults to what the index can address (8192 with Fix B, else 4096) and is refused above it |
| `CIVFIX_NO_RETIREGUARD=1` | skip Fixes D and E |
| `CIVFIX_RES_SINK=1` | enable the resource sink — **known to crash**, experiments only |

## Log

`LargeMapFix.log`, written beside the DLL. Expect at startup:

```
[capfix] host: CivilizationVI_DX12.exe
[widen] mode 2 (CivFixWiden.txt=2, CIVFIX_WIDEN=unset)
[widen] verified all 320 sites against the loaded image
[widen] mode 2: relocated the resource list from module+0x6DB3840 to ... handle limit = 65535
[capfix] index13: 6 of 6 mask sites widened 0xFFF -> 0x1FFF in DllMain, before the
         game could produce a record - the terrain descriptor index is now 13 bits
         (8192 addressable slots, up from 4096)
[capfix] descriptor limit = 8192
[capfix] heap: sink buffers come from the game's own imported _aligned_malloc ...
[capfix] descriptor allocator: located at ... check bytes verified at +0xA8
[capfix] AddDynamicResource: located at ... check bytes verified at +0x62
[capfix] retire queue A (0x144491600): located at ... check bytes verified at +0x12B
[capfix] retire queue B (0x144491618): located at ... check bytes verified at +0x12B
[capfix] retire guards: 2 of 2 installed
[capfix] collection walker (0x140954490): located at ... check bytes verified at +0x20
```

During play, `res: exhausted=0` and `desc: capped=0` say both limits are holding.

`MISSED SITE` or `POISON DISTURBED` from `[widen]` means a reference to the relocated
object was not rewritten. That is the one failure worth stopping for.

`SINK DISABLED`, `NOT PATCHED` or `NOT HOOKED` means a patch declined to apply because a
signature or check byte did not match — most likely a game update. The fix degrades to
original behaviour rather than writing to an address it has not confirmed.

`WRONG EXECUTABLE` means the host is `CivilizationVI.exe`, the DirectX 11 build. Both
executables import `version.dll`, so the proxy loads into either, but the patch sites are
DX12 addresses and the two are different code (`.text` `0xFA3FCC` against `0xF94E2C`).
Nothing applies. The name is checked purely so the log can say this — the site
verification remains the authority, so an unrecognised name never blocks a patch and a
renamed executable still works.

## Version coupling

Everything fails **closed**. Every site is verified against the loaded image before
anything is written, and a mismatch means nothing is applied rather than something
half-applied — a half-widened field set would leave producers and consumers disagreeing
about records already on the heap, which is worse than not patching at all.

The two hooks are located by byte signature with a corroborating check pattern at a fixed
offset, so they tolerate minor patches. Fix A's 320 sites and Fix B's 6 mask sites are
hardcoded RVAs, each verified before use. A game update will likely invalidate the RVAs first; the log will say so, and
those patches will simply not install.
