# Civ VI large-map crash fix — `version.dll` proxy

Civilization VI crashes on maps much larger than the vanilla ceiling. Two hard limits in
the renderer cause it, and this fixes both by patching the running process — no injector,
no mod manager, no Steam launch options. It loads automatically every launch.

Developed against **180×94** and **200×100** (the largest the game offers). The folklore
limit is 128×80.

**[LARGE-MAP-FIX.md](LARGE-MAP-FIX.md)** has the full diagnosis: the faulting
instruction, the caller chain, the evidence, and why each patch is narrow.

> [!NOTE]
> **This was built primarily by Claude (Anthropic's LLM).** The reverse engineering —
> locating the limits, tracing the crashes, working out the relocation plan — and
> essentially all of the code and documentation here are its work. My part was mostly
> direction, running the game, and feeding back crash dumps and logs, so I take little
> credit for it. Bear that in mind when reading the analysis: it is unusually thorough,
> but it is machine-derived, and the claims that matter are the ones the DLL verifies
> against the loaded image at startup rather than the ones written in prose.

## Requirements

> [!IMPORTANT]
> **Windows only, and the DirectX 12 executable specifically.**

| | |
| --- | --- |
| OS | **Windows x64.** The whole mechanism is a Windows DLL proxy — there is no equivalent to port. |
| Game | The **Steam** build, launched as **DirectX 12** (`CivilizationVI_DX12.exe`). |
| Not supported | The macOS and Linux (Aspyr) ports — different executables entirely. |
| Untested | Linux via Proton or Steam Deck. It runs the Windows executable, so this is not obviously impossible, but Wine resolves DLLs by its own rules and would likely need a `WINEDLLOVERRIDES` entry. Nobody has tried it. |

**The DX11 vs DX12 choice matters and is easy to get wrong.** Civ VI ships two
executables, and its launcher asks which to use. Both import `version.dll`, so the proxy
loads either way — but every patch site is an address in the DX12 binary, and the two
have different code (`.text` is `0xFA3FCC` in DX12, `0xF94E2C` in DX11). Launch DX11 and
every site fails its verification, nothing is applied, and the game crashes exactly as it
did before.

The DLL detects this itself and says so at the top of `LargeMapFix.log`, so you do not
have to work it out from the mismatch count:

```
[capfix] *** WRONG EXECUTABLE *** host is CivilizationVI.exe. Civ VI ships two
         executables with different code, and everything here targets
         CivilizationVI_DX12.exe. Nothing below will apply.
```

On the right executable that line reads `[capfix] host: CivilizationVI_DX12.exe`.

## What it fixes

| # | limit | stock | after |
| --- | --- | --- | --- |
| 1 | renderer resource handles | 32768 | **65534** |
| 2 | terrain descriptor index | 4096 (12 bits) | **8192** (13 bits) |

**Limit 1 is the upstream cause of nearly everything else.** Handles are `uint16`,
allocated meet-in-the-middle: statics count up from 1, dynamics count down from `0x8000`.
So `0x8000` is *half* the range, not the end of it. The backing arrays are inline in a
6.9 MB static, so the object is **relocated** to a doubled block and all **320**
references to it are rewritten in place. Every edit is a 4-byte field inside an
instruction that keeps its length, so nothing is relocated and no trampoline exists.

**Limit 2** is a width mismatch. The allocator returns a 16-bit slot index and the chunk
loader stores 16 bits, but the packed render record carries 12 and every consumer masks
to `0xFFF`. Past 4096 descriptors the index wraps onto unrelated geometry. Bit 12 of that
field turns out to be a hardcoded constant nothing reads, so **six `imm32` masks widen to
`0x1FFF`** and the field becomes 13 bits. Measured peak demand on a fully revealed
ludicrous map was ~6055, under 8192.

A **descriptor cap** still runs as a backstop behind limit 2 — if something ever does
exceed 8192, the overflow renders blank instead of crashing. It sits idle in normal play.

Two smaller guards remain for the deferred-destroy queues and the collection walker; both
validate a pointer the game dereferences unchecked. See LARGE-MAP-FIX.md.

### Tested

- A large map played through with no crash, `exhausted=0` and `capped=0`.
- **Gathering Storm rising sea level**, through an actual flooding event — no crash. That
  one matters: sea level rise re-generates coastal terrain mid-game, which drives the
  descriptor allocator and the renderer harder than ordinary play.

## Install

Download `version.dll` from the [latest release](../../releases/latest) into the repo's
`build\` folder, then run `install.bat`. Or build it yourself:

```
build.bat
install.bat
```

Building needs Visual Studio 2022 with the C++ desktop workload. `install.bat` finds the
game in any Steam library on any drive; pass the folder if it can't:

```
install.bat "D:\Games\Sid Meier's Civilization VI\Base\Binaries\Win64Steam"
```

It puts three files in the game folder:

| file | what it is |
| --- | --- |
| `version.dll` | this proxy — forwards every export and loads the fix |
| `version_orig.dll` | a copy of `C:\Windows\System32\version.dll`, the real one |
| `CivFixWiden.txt` | `2`, enabling the handle widening |

`uninstall.bat` removes all of them. Everything here is a **memory patch**: nothing is
written to your saves, settings, or the game's own files, so uninstalling reverts you to
stock.

### It will refuse to install over another mod

If a `version.dll` is already there without a `version_orig.dll`, it belongs to something
else — another proxy mod, ReShade, a script extender — and `install.bat` stops rather
than breaking it. Sort that out first.

## Checking it worked

`LargeMapFix.log` appears in the game folder. Expect:

```
[capfix] host: CivilizationVI_DX12.exe
[widen] mode 2 (CivFixWiden.txt=2, ...)
[widen] verified all 320 sites against the loaded image
[widen] mode 2: relocated the resource list from module+0x6DB3840 to ... handle limit = 65535
[capfix] index13: 6 of 6 mask sites widened 0xFFF -> 0x1FFF in DllMain ...
[capfix] descriptor limit = 8192
[capfix] descriptor allocator: located at ... check bytes verified at +0xA8
```

Then during play, `res: exhausted=0` and `desc: capped=0` are the two numbers that say
both limits are holding.

**`MISSED SITE` or `POISON DISTURBED` from `[widen]` is the one failure worth stopping
for** — it means a reference to the relocated object was not rewritten.

`NOT PATCHED` or `NOT HOOKED` means a patch declined to apply because a byte signature or
check pattern did not match — almost always a game update. It then leaves that code path
at original behaviour rather than writing to an address it has not confirmed.

## Configuration

Two files beside the DLL, both read at every start. **Use these rather than environment
variables** for anything that matters: Steam launches the game with the environment
*Steam itself* inherited, so a changed variable does nothing until Steam is restarted.

| file | effect |
| --- | --- |
| `CivFixWiden.txt` | `0`/absent = off; `1` = relocate the resource list and guard the old region with the limit unchanged (the validation run); **`2` = also raise the handle limit to 65534** |
| `CivFixIndex13.txt` | `0` = keep the descriptor index at 12 bits and the cap at 4096; absent = widen to 13 bits |

Environment variables, all optional:

| variable | effect |
| --- | --- |
| `CIVFIX_DESC_LIMIT` | descriptor cap; defaults to what the index can address (8192 widened, else 4096) and is refused above it |
| `CIVFIX_NO_RETIREGUARD=1` | skip the deferred-destroy queue and collection-walker guards |
| `CIVFIX_RES_SINK=1` | enable the resource sink — **known to crash**, experiments only |

## Why `version.dll`

Verified against this build, not assumed:

- `CivilizationVI_DX12.exe` **imports it directly**, so it loads during process
  initialisation — before anything renders. That matters: the handle widening has to land
  before the resource list's constructor runs, and the constructor is one of the exe's own
  CRT initialisers.
- It is **not** in the `KnownDLLs` registry list, so a copy beside the executable wins
  the module search order. (`dxgi.dll` and `d3d12.dll` would also work, but they collide
  with ReShade and overlays; `version.dll` has nothing to do with graphics.)
- Small, stable export surface: 17 functions, of which the game uses 6.

Every export is a **linker forwarder** to `version_orig.dll` rather than a hand-written
wrapper. That is a correctness decision: 16 of the 17 are documented, but
`GetFileVersionInfoByHandle` is not, and a guessed signature would corrupt the stack of
whoever calls it. Forwarding is exact for all 17. Ordinals are pinned to match the real
DLL so by-ordinal imports still resolve.

`DllMain` does two things only: apply the handle widening (which cannot wait — see above,
and it touches no CRT, no allocator and no file streams) and `CreateThread`. Everything
else runs on that thread once the loader lock is released.

## Version coupling

This patches one specific build. It is written to fail *closed*: every site is verified
against the loaded image before anything is written, and a mismatch means nothing is
applied rather than something half-applied.

- The 320 widening sites and the 6 index-mask sites are **hardcoded RVAs**, each checked
  against the bytes that must already be there. All are verified before any is written.
- The hooks are located by **byte signature** with a corroborating check pattern at a
  fixed offset, so they survive minor patches.

A game update will most likely invalidate the RVAs first. The log will say so, and those
patches simply will not install.

Developed against the Steam DX12 build with `.text` at `0x140001000`, size `16400332`.

## Layout

```
build.bat                build -> build\version.dll, then verify its exports
install.bat              copy into the game folder (refuses to clobber another proxy)
uninstall.bat            remove
src\proxy.cpp            the 17 export forwarders + DllMain
src\Widen.cpp/.h         limit 1: relocate the resource list, raise the handle ceiling
src\WidenTable.h         the 320 relocation sites (generated)
src\LargeMapFix.cpp/.h   limit 2, the descriptor cap, and the remaining guards
src\Index13Table.h       the 6 descriptor-index mask sites (generated)
LARGE-MAP-FIX.md         full diagnosis
RELEASE_NOTES.md         notes for the next release, used by the workflow
.github\workflows\       release build
```

Both `*Table.h` files are generated from a disassembly of the game executable and are
checked in because the generators need a copy of that executable to run. Each entry
carries the bytes that must already be present, so a wrong build is caught at load.

`build.bat` checks its own output before declaring success: all 17 exports present and
every one a true forwarder to `version_orig`. A mistyped `/export` pragma otherwise
produces a DLL that stops the game starting, with nothing to point at why. If the check
fails the DLL is deleted rather than left to be installed.

## Releases

**Pushing a version tag** builds `version.dll` on a clean Windows runner and publishes a
release with the DLL and its `.sha256` attached. Cutting one:

```
edit RELEASE_NOTES.md
git tag v1.2.0
git push origin v1.2.0
```

Don't create the release by hand — the workflow does it. Build first, release second: a
release that already exists cannot always accept new assets, so the DLL has to be ready
before the release is created. Bare `1.2.0` tags work too.

Release notes come from `RELEASE_NOTES.md`, versioned alongside the code. `Actions ->
Release build -> Run workflow` rebuilds against an existing tag if you need it.

Each release DLL gets a signed build provenance attestation, so anyone can confirm the
binary came from this repo at that commit rather than trusting the upload:

```
gh attestation verify version.dll --repo <owner>/<repo>
```

That is worth doing. A DLL you drop next to a game executable is indistinguishable from
malware by inspection, and "trust me" is not a good answer.

## Licence

MIT — see [LICENSE](LICENSE).
