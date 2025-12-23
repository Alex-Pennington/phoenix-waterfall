# Phoenix Waterfall - Copilot Instructions

## P0 - CRITICAL RULES

1. **NO unauthorized additions.** Do not add features, flags, modes, or files without explicit instruction. ASK FIRST.
2. **Minimal scope.** Fix only what's requested. One change at a time.
3. **Suggest before acting.** Explain your plan, wait for confirmation.
4. **Verify before modifying.** Run `git status`, read files before editing.

---

## Architecture Overview

**Phoenix Waterfall** is the SDL2-based **display-only** visualization for the Phoenix SDR suite. It provides visual feedback to the operator showing RF signal quality. Detection logic (tick detection, BCD decoding, etc.) lives in a separate module.

### Signal Flow
```
signal_relay (TCP:4411)
        │
        │  12kHz float32 I/Q
        ▼
   waterfall.c
        │
        ├──────────────────┐
        │                  │
        ▼                  ▼
   FFT Processing    Audio Output
        │            (waterfall_audio)
        ▼
   Waterfall Display
   (color-mapped spectrum)
```

### Key Files
| File | Purpose |
|------|---------|
| `src/waterfall.c` | Main application, SDL rendering, TCP client |
| `src/waterfall_dsp.c` | DSP utilities (lowpass, DC removal) |
| `src/waterfall_audio.c` | Audio output for operator monitoring |
| `src/ui_core.c` | GUI framework |
| `src/ui_widgets.c` | Input, slider, button widgets |
| `src/kiss_fft.c` | FFT processing |

---

## Build

```powershell
cd build/msys2-ucrt64
cmake --build .
```

**Dependencies:** SDL2, SDL2_ttf, kiss_fft (included), phoenix-discovery (submodule)

---

## Key Patterns

### P1 - TCP Protocol (signal_relay)
```c
// Stream header (sent once on connect)
typedef struct {
    uint32_t magic;        // 0x46543332 "FT32"
    uint32_t sample_rate;  // 12000
    uint32_t reserved1;
    uint32_t reserved2;
} relay_stream_header_t;

// Data frames (continuous)
typedef struct {
    uint32_t magic;        // 0x44415441 "DATA"
    uint32_t sequence;
    uint32_t num_samples;
    uint32_t reserved;
    // followed by num_samples * 2 * float32 (I/Q pairs)
} relay_data_frame_t;
```

### P2 - Service Discovery
Uses phoenix-discovery submodule for LAN service discovery:
```c
pn_announce(node_id, "waterfall", 0, 0, "display");
pn_listen(on_service_discovered, NULL);
```
Auto-connects to `signal_splitter` when discovered.

### P3 - DSP Filter Pattern
```c
wf_lowpass_t lp;
wf_lowpass_init(&lp, cutoff_hz, sample_rate);
float out = wf_lowpass_process(&lp, input);
```

### P4 - Display Parameters
```c
#define DISPLAY_SAMPLE_RATE     12000
#define DISPLAY_FFT_SIZE        2048
#define DISPLAY_OVERLAP         1024
#define DISPLAY_HZ_PER_BIN      5.86f  // 12000/2048
#define ZOOM_MAX_HZ             5000.0f
```

---

## Runtime Controls

| Key | Action |
|-----|--------|
| Tab | Toggle settings panel |
| +/- | Adjust gain |
| R | Reconnect |
| T | Toggle test pattern |
| Q/Esc | Quit |

---

## Configuration

Settings persist to `waterfall.ini`:
```ini
host=localhost
port=4411
width=1024
height=600
gain=0.0
```

---

## Dependencies

| Library | Purpose |
|---------|---------|
| SDL2 | Graphics and window |
| SDL2_ttf | Text rendering |
| kiss_fft | FFT processing (included) |
| phoenix-discovery | LAN service discovery (submodule) |
| Winsock2 | TCP networking (Windows) |
