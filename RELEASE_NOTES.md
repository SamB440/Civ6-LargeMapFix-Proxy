Fixes Civilization VI crashes on maps larger than the vanilla ceiling, by widening two
hard limits in the renderer at load time. Developed against 180×94 and 200×100, the
largest the game offers; the folklore limit is 128×80.

## What it fixes

| limit | stock | after |
| --- | --- | --- |
| renderer resource handles | 32768 | **65534** |
| terrain descriptor index | 4096 (12 bits) | **8192** (13 bits) |

The handle space is the upstream cause of most of the crashes. Handles are `uint16`
issued meet-in-the-middle, so `0x8000` is half the range rather than the end of it; the
backing arrays are inline in a 6.9 MB static, so the object is relocated to a doubled
block and all 320 references rewritten in place. The descriptor index is a width
mismatch — the allocator returns 16 bits but the packed render record carries 12, so past
4096 descriptors the index wraps onto unrelated geometry.

[LARGE-MAP-FIX.md](../blob/master/LARGE-MAP-FIX.md) has the full diagnosis of both, plus
the guards behind them.

## Requirements

- **Windows x64.** The whole mechanism is a Windows DLL proxy.
- **The Steam build, launched as DirectX 12.** Civ VI ships two executables and the
  launcher asks which to use. The DX11 one is different code, so nothing applies there —
  the log says `*** WRONG EXECUTABLE ***` if you land on it.
- Not the macOS/Linux (Aspyr) ports. Proton and Steam Deck are untested.

## Install

Download `version.dll` below into the repo's `build\` folder, then run `install.bat`.
Or build from source with `build.bat`. Either way `install.bat` finds the game in any
Steam library on any drive, and refuses to overwrite another mod's `version.dll`.

Everything is a **memory patch** — nothing is written to your saves, settings, or the
game's files. `uninstall.bat` reverts you to stock.

## Verifying the download

A proxy DLL that sits next to a game executable is indistinguishable from malware by
inspection, so don't just trust the upload:

```
gh attestation verify version.dll --repo SamB440/Civ6-LargeMapFix-Proxy
```

That checks a signed build provenance attestation tying this exact binary to the tagged
commit and the workflow run that produced it. `version.dll.sha256` is attached as well,
and the run log is the build record. Building from source with `build.bat` remains the
strongest guarantee.

## Checking it worked

`LargeMapFix.log` appears beside the DLL. Expect `handle limit = 65535`, `6 of 6 mask
sites widened`, and then `exhausted=0` / `capped=0` during play. `MISSED SITE` or
`POISON DISTURBED` is the one failure worth stopping for.

## Credit

Built primarily by Claude (Anthropic's LLM) — the reverse engineering, the code, and the
documentation. See the note at the top of the README.
