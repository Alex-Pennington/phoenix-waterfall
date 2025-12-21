# Phoenix Waterfall - Copilot Instructions

## P0 - CRITICAL RULES

1. **NO unauthorized additions.** Do not add features, flags, modes, or files without explicit instruction. ASK FIRST.
2. **Minimal scope.** Fix only what's requested. One change at a time.
3. **Suggest before acting.** Explain your plan, wait for confirmation.
4. **Verify before modifying.** Run `git status`, read files before editing.

---

## Architecture Overview

**Phoenix Waterfall** is the SDL2-based visualization and detection application for WWV/WWVH time signals.

### Signal Flow
```
TCP I/Q Input (from phoenix-sdr-core)
        │
        ▼
   waterfall.c
        │
        ├─────────────────────────────────────────────┐
        │                                             │
        ▼                                             ▼
DETECTOR PATH (50 kHz)                     DISPLAY PATH (12 kHz)
lowpass → decimate                         lowpass → decimate
        │                                             │
   ┌────┼────┬────────────┐                ┌──────────┤
   ▼    ▼    ▼            ▼                ▼          ▼
 tick marker bcd_*     sync          tone_tracker  FFT waterfall
   │    │    │          │                  │
   ▼    ▼    ▼          ▼                  ▼
callbacks → CSV logging + UDP telemetry
```

### Key Directories
| Path | Purpose |
|------|---------|
| `src/` | Main application and detector modules |
| `include/` | Public headers |
| `docs/` | Documentation |

---

## Build

```powershell
.\build.ps1                    # Debug build
.\build.ps1 -Release           # Optimized build
.\build.ps1 -Clean             # Clean artifacts
```

**Dependencies:** SDL2, kiss_fft (included), Winsock2

---

## Critical Patterns

### P1 - Signal Path Divergence
All signal processors receive samples from the SAME divergence point in `waterfall.c`:
```c
// Raw samples normalized to [-1, 1]
float i_raw = (float)samples[s * 2] / 32768.0f;
float q_raw = (float)samples[s * 2 + 1] / 32768.0f;

// DETECTOR PATH (50 kHz) - for pulse detection
float det_i = lowpass_process(&g_detector_lowpass_i, i_raw);
float det_q = lowpass_process(&g_detector_lowpass_q, i_raw);
// → tick_detector, marker_detector, bcd_*_detector

// DISPLAY PATH (12 kHz) - for visualization
float disp_i = lowpass_process(&g_display_lowpass_i, i_raw);
float disp_q = lowpass_process(&g_display_lowpass_q, q_raw);
// → tone_tracker, bcd_envelope, FFT waterfall
```
**Never cross paths.** Detectors use `det_i/det_q`. Display uses `disp_i/disp_q`.

### P2 - Detector Module Pattern
All detectors follow the same structure:
```c
// Header pattern
typedef struct xxx_detector xxx_detector_t;           // Opaque type
typedef void (*xxx_callback_fn)(const xxx_event_t *event, void *user_data);

xxx_detector_t *xxx_detector_create(const char *csv_path);  // NULL disables CSV
void xxx_detector_destroy(xxx_detector_t *det);
void xxx_detector_set_callback(xxx_detector_t *det, xxx_callback_fn cb, void *user_data);
bool xxx_detector_process_sample(xxx_detector_t *det, float i, float q);
```
Each detector owns: own FFT, own sample buffer, own state machine.

### P3 - CSV/UDP Telemetry Pattern
Detectors support dual output:
```c
// CSV header pattern (written in _create)
fprintf(csv_file, "# Phoenix SDR %s Log v%s\n", detector_name, VERSION);
fprintf(csv_file, "time,timestamp_ms,field1,field2,...\n");

// UDP telemetry pattern (from callbacks)
telem_sendf(TELEM_TICKS, "%s,%.1f,T%d,%d,...", time_str, timestamp_ms, tick_num, ...);
```
Channel prefixes: `TICK`, `MARK`, `SYNC`, `BCDS`, `CARR`, `T500`, `T600`

### P4 - Display/Audio Isolation
Display and audio paths NEVER share variables or filters:
- Display: `g_display_dsp`
- Audio: `g_audio_dsp` (in `waterfall_audio.c`)

### P5 - DSP Filter Pattern
Filters use inline structs from `waterfall_dsp.h`:
```c
wf_lowpass_t lp;
wf_lowpass_init(&lp, cutoff_hz, sample_rate);
float out = wf_lowpass_process(&lp, input);
```

### P7 - Callback Event Structs
Each detector defines an event struct passed to callbacks:
```c
typedef struct {
    float timestamp_ms;     // When event occurred
    float duration_ms;      // Pulse/event duration
    float peak_energy;      // Signal strength
    // ... detector-specific fields
} xxx_event_t;
```

### P8 - UI Flash Feedback Pattern
Detectors provide flash state for visual feedback via `waterfall_flash.h`:
```c
int xxx_detector_get_flash_frames(xxx_detector_t *det);
void xxx_detector_decrement_flash(xxx_detector_t *det);
```
Flash sources register with `flash_register()` for waterfall band markers.

### P9 - WWV Broadcast Clock
WWV/WWVH broadcast schedule knowledge:
- Tick at each second EXCEPT seconds 29 and 59
- 800ms marker at second 0 of each minute
- Station-aware: WWV=1000Hz, WWVH=1200Hz

---

## Thawed Files

All files are open for modification. The repository split provides a clean slate.

Previously frozen files that are now thawed:
- `src/waterfall.c`
- `src/waterfall_dsp.c`
- `src/marker_detector.c`
- `src/sync_detector.c`

---

## Domain Knowledge

- **WWV/WWVH:** NIST time stations at 5/10/15/20 MHz
- **Tick:** 5ms pulse of 1000 Hz tone every second (AM modulated)
- **Minute marker:** 800ms pulse at second 0 of each minute
- **BCD time code:** 100 Hz subcarrier with binary time data
- **DC hole:** Zero-IF receivers have DC offset; tune 450 Hz off-center

---

## Dependencies

| Library | Purpose |
|---------|---------|
| SDL2 | Graphics and audio |
| kiss_fft | FFT processing (included) |
| Winsock2 | TCP/UDP networking |
