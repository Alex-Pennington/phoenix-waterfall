# phoenix-waterfall

**Version:** v0.2.0  
**Part of:** Phoenix Nest MARS Communications Suite  
**Developer:** Alex Pennington (KY4OLB)

---

## Overview

Real-time SDR waterfall display for the Phoenix SDR suite. Receives 12kHz float32 I/Q samples from signal_relay and provides visual spectrum analysis for operator signal verification.

This is the **display chain only** — detection logic (WWV tick detection, BCD decoding, etc.) lives in a separate module.

---

## Features

- **SDL2 Waterfall Display** — Real-time spectrum visualization
- **Auto-Gain** — Adaptive color mapping
- **Service Discovery** — Auto-connects to signal_splitter via UDP discovery
- **Settings Panel** — Runtime configuration (Tab key)
- **Test Pattern** — 1000 Hz tone for testing without network
- **Resizable Window** — Configuration persists to INI file

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                        phoenix-waterfall                             │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   signal_relay (TCP:4411)                                           │
│        │                                                            │
│        │  12kHz float32 I/Q                                         │
│        ▼                                                            │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐             │
│  │   I/Q       │───►│     FFT     │───►│  Waterfall  │             │
│  │   Buffer    │    │  Processing │    │   Display   │             │
│  └─────────────┘    └─────────────┘    └─────────────┘             │
│                                                                     │
│   phoenix-discovery                                                 │
│        │                                                            │
│        └── Auto-connect to signal_splitter                          │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Dependencies

### Required

- **SDL2** — Graphics and window management
- **SDL2_ttf** — Text rendering
- **kiss_fft** — FFT processing (included)
- **phoenix-discovery** — LAN service discovery (submodule)

### Windows Build

- MSYS2 UCRT64 or MinGW-w64
- CMake 3.16+

---

## Building

### Quick Build (MSYS2)

```bash
cd build/msys2-ucrt64
cmake --build .
```

### Full Rebuild

```bash
cd build/msys2-ucrt64
cmake --build . --clean-first
```

---

## Usage

### Auto-Discovery Mode

```powershell
# Start signal_splitter (broadcasts discovery)
signal_splitter.exe

# Start waterfall (auto-connects)
waterfall.exe
```

### Manual Connection

```powershell
waterfall.exe --host 192.168.1.100 --port 4411
```

### Command Line Options

```
waterfall.exe [options]

Options:
  --host HOST       Relay server hostname (default: localhost)
  --port PORT       Display stream port (default: 4411)
  --test-pattern    Generate test tone (no network)
  --node-id ID      Node ID for discovery (default: WATERFALL-1)
  --no-discovery    Disable service discovery
  --no-auto         Disable auto-connect to discovered services
  --help            Show this help
```

### Keyboard Controls

| Key | Action |
|-----|--------|
| `Tab` | Toggle settings panel |
| `+` / `=` | Gain up |
| `-` | Gain down |
| `R` | Reconnect |
| `T` | Toggle test pattern |
| `Q` / `ESC` | Quit |

---

## Configuration

Settings saved to `waterfall.ini`:

```ini
; Phoenix Waterfall Configuration
host=localhost
port=4411
width=1024
height=600
gain=0.0
```

---

## Signal Protocol

### Stream Header (FT32)

```c
typedef struct {
    uint32_t magic;        // 0x46543332 "FT32"
    uint32_t sample_rate;  // 12000
    uint32_t reserved1;
    uint32_t reserved2;
} relay_stream_header_t;
```

### Data Frames (DATA)

```c
typedef struct {
    uint32_t magic;        // 0x44415441 "DATA"
    uint32_t sequence;
    uint32_t num_samples;
    uint32_t reserved;
    // float32 I/Q pairs follow
} relay_data_frame_t;
```

---

## Source Files

| File | Description |
|------|-------------|
| `src/waterfall.c` | Main application, SDL rendering |
| `src/waterfall_dsp.c` | DSP utilities (lowpass, DC removal) |
| `src/waterfall_audio.c` | Audio output |
| `src/ui_core.c` | GUI framework |
| `src/ui_widgets.c` | Widget implementations |
| `src/kiss_fft.c` | FFT processing |

---

## Related Repositories

| Repository | Description |
|------------|-------------|
| [mars-suite](https://github.com/Alex-Pennington/mars-suite) | Phoenix Nest MARS Suite index |
| [phoenix-sdr-core](https://github.com/Alex-Pennington/phoenix-sdr-core) | SDR hardware interface |
| [phoenix-discovery](https://github.com/Alex-Pennington/phoenix-discovery) | LAN service discovery |

---

## License

AGPL-3.0 — See [LICENSE](LICENSE)

---

*Phoenix Nest MARS Communications Suite*  
*KY4OLB*
