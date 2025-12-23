# Phoenix Waterfall — Copilot Instructions

## P0 — Critical Rules
- No unauthorized additions: do not add features/files without explicit approval.
- Minimal scope: fix only what’s requested, one change at a time.
- Suggest first: state plan briefly before edits; get confirmation when ambiguous.
- Verify before modifying: run `git status` and read the relevant files.

## Big Picture
- Display-only SDL2 app that visualizes 12 kHz float32 I/Q from `signal_relay` (TCP:4411).
- Waterfall path: I/Q buffer → FFT (phoenix-kiss-fft) → color-mapped spectrum.
- Audio path: optional monitor via `src/waterfall_audio.c`; detection/decoding lives elsewhere.

## Key Files
- `src/waterfall.c`: SDL init, TCP client, FFT, render loop.
- `src/waterfall_audio.c`: AM demod/audio monitor.
- `src/ui_core.c`, `src/ui_widgets.c`: lightweight GUI and widgets.
- `waterfall.ini`: persisted settings (host/port/size/gain).

## Submodules (external/)
- `phoenix-discovery`: UDP LAN discovery; auto-connect to `signal_splitter`.
- `phoenix-dsp`: primitives (lowpass, DC block, AM demod).
- `phoenix-kiss-fft`: FFT implementation used by the display.

## Build & Release (phoenix-build-scripts only)
- For any build, use the deploy script (no direct CMake):
	- Dry run (build + package, no upload):
		- `external/phoenix-build-scripts/scripts/deploy-release.ps1 -IncrementPatch`
	- Deploy to GitHub:
		- `external/phoenix-build-scripts/scripts/deploy-release.ps1 -IncrementPatch -Deploy`
- Packaging is defined by [phoenix-build.json](phoenix-build.json): `executables`, `dlls`, `packageFiles` collected from `build/msys2-ucrt64/`.
- Version header is auto-generated to `build/include/version.h` from [cmake/version.h.in](cmake/version.h.in); do not edit manually.

### Windows Environment
- MSYS2 UCRT64 with CMake 3.16+ and Ninja; GitHub CLI (`gh`) configured for release uploads.

## Runtime & Controls
- Keys: Tab (settings), +/- (gain), R (reconnect), T (test tone), Q/ESC (quit).
- Manual connect: `--host` and `--port` CLI; discovery can be disabled via flags.
- Typical params: `DISPLAY_FFT_SIZE=2048`, overlap=1024, ~5.86 Hz/bin at 12 kHz.

## Protocol (relay)
- Header `FT32`: `magic=0x46543332`, `sample_rate=12000`.
- Frames `DATA`: `magic=0x44415441`, `sequence`, `num_samples`, followed by float32 I/Q pairs.

## Patterns to Follow
- Discovery: `pn_announce(..."waterfall"...)` and `pn_listen(on_service_discovered, ...)`.
- DSP usage: `pn_lowpass_init/process`, `pn_am_demod_init/process` from `pn_dsp.h`.
- FFT: use phoenix-kiss-fft (`kiss_fft*`) via included headers.

## Gotchas
- This is display-only; do not add detection/decoder logic here.
- SDL2/SDL2_ttf are vendored under `libs/`; Winsock2 used for TCP on Windows.
- Prefer minimal, surgical changes; match existing style and avoid refactors unless asked.
