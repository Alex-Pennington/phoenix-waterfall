# Phoenix Waterfall — Copilot Instructions

## P0 — Critical Rules
- No unauthorized additions: do not add features/files without explicit approval.
- Minimal scope: fix only what’s requested, one change at a time.
- Suggest first: state plan briefly before edits; get confirmation when ambiguous.
- Verify before modifying: run `git status` and read the relevant files.

## Big Picture
- Display-only SDL2 app that visualizes raw I/Q from `sdr_server`.
- Auto-discovers sdr_server via phoenix-discovery, auto-connects with zero configuration.
- Auto-reconnects on disconnect (5 second retry interval).
- Waterfall path: I/Q buffer → Decimation (2 MHz → 12 kHz) → FFT (phoenix-kiss-fft) → color-mapped spectrum.
- Audio path: optional monitor via `src/waterfall_audio.c`; detection/decoding lives elsewhere.

## Key Files
- `src/waterfall.c`: SDL init, TCP client, FFT, render loop.
- `src/waterfall_audio.c`: AM demod/audio monitor.
- `src/ui_core.c`, `src/ui_widgets.c`: lightweight GUI and widgets.
- `waterfall.ini`: persisted settings (host/port/size/gain).

## Submodules (external/)
- `phoenix-discovery`: UDP LAN discovery; auto-find and auto-connect to `sdr_server`.
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
- Auto-connects to sdr_server via discovery (zero configuration).
- Auto-reconnects on disconnect every 5 seconds.
- Keys: +/- (gain), Tab (settings), Q/ESC (quit).
- Discovery can be disabled via `--no-discovery` flag; manual host/port via `--host`/`--port` CLI.

## Protocol (sdr_server)
- PHXI/IQDQ streaming protocol from sdr_server data_port (discovered via phoenix-discovery).
- PHXI header (32 bytes): sample_rate, sample_format (S16/F32/U8), center_freq, gain, LNA state.
- IQDQ data frames (16-byte header + samples): sequence number, sample count, I/Q data.
- META frames (32 bytes): runtime parameter updates (frequency, gain, LNA).
- Sample format conversion via phoenix-dsp: pn_s16_to_float(), pn_u8_to_float().
- Decimation via phoenix-dsp: pn_decimate_t with anti-aliasing (2 MSPS → 12 kHz display rate).

## Patterns to Follow
- Discovery: `pn_announce(..."waterfall"...)` and `pn_listen(on_service_discovered, ...)`.
- DSP usage: `pn_lowpass_init/process`, `pn_am_demod_init/process` from `pn_dsp.h`.
- FFT: use phoenix-kiss-fft (`kiss_fft*`) via included headers.

## Gotchas
- This is display-only; do not add detection/decoder logic here.
- SDL2/SDL2_ttf are vendored under `libs/`; Winsock2 used for TCP on Windows.
- Prefer minimal, surgical changes; match existing style and avoid refactors unless asked.
