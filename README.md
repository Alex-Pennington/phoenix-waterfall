# phoenix-waterfall

**Version:** v0.1.0  
**Part of:** Phoenix Nest MARS Communications Suite  
**Developer:** Alex Pennington (KY4OLB)

---

## Overview

Real-time SDR waterfall display with integrated WWV time signal detection. Receives I/Q samples from phoenix-sdr-core and provides visual spectrum analysis with synchronized audio output.

---

## Features

- **SDL2 Waterfall Display** — Real-time spectrum visualization
- **WWV Tick Detection** — 1000 Hz tick pulse detection at 1-second intervals
- **Minute Marker Detection** — 800ms marker detection for minute boundaries
- **BCD Time Decoder** — 100 Hz subcarrier time code extraction
- **Tone Tracking** — 500/600 Hz reference tone identification
- **Sync State Machine** — Multi-stage synchronization with confidence tracking
- **UDP Telemetry** — Real-time broadcast of detection events
- **Audio Output** — Demodulated audio with volume control

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                        phoenix-waterfall                             │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   TCP I/Q Input                                                     │
│        │                                                            │
│        ▼                                                            │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐             │
│  │   Decimate  │───►│  FFT/DSP    │───►│  Waterfall  │             │
│  │   2M→50k    │    │  Processing │    │   Display   │             │
│  └─────────────┘    └─────────────┘    └─────────────┘             │
│        │                                                            │
│        ├────────────────────────────────────────────────┐          │
│        │                                                │          │
│        ▼                                                ▼          │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐            │
│  │   Tick      │    │   Marker    │    │    BCD      │            │
│  │  Detector   │    │  Detector   │    │  Decoder    │            │
│  └──────┬──────┘    └──────┬──────┘    └──────┬──────┘            │
│         │                  │                  │                    │
│         └──────────────────┼──────────────────┘                    │
│                            ▼                                        │
│                    ┌─────────────┐                                 │
│                    │    Sync     │                                 │
│                    │  Detector   │                                 │
│                    └──────┬──────┘                                 │
│                           │                                         │
│                           ▼                                         │
│                    ┌─────────────┐                                 │
│                    │  Telemetry  │──► UDP Broadcast                │
│                    │   Output    │                                 │
│                    └─────────────┘                                 │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Dependencies

### Required

- **SDL2** — Graphics and audio
- **kiss_fft** — FFT processing (included)
- **phoenix-sdr-core** — I/Q streaming source

### Windows Build

- MinGW-w64 or MSVC
- SDL2 development libraries

---

## Building

### Quick Build

```powershell
.\build.ps1
```

### Manual Build

```powershell
gcc -O2 -I include -I "C:\SDL2\include" ^
    src\waterfall.c src\waterfall_dsp.c src\waterfall_flash.c ^
    src\waterfall_telemetry.c src\waterfall_audio.c ^
    src\tick_detector.c src\marker_detector.c src\sync_detector.c ^
    src\tone_tracker.c src\bcd_*.c src\channel_filters.c ^
    src\kiss_fft.c ^
    -L "C:\SDL2\lib" -lSDL2main -lSDL2 -lws2_32 -lwinmm ^
    -o waterfall.exe
```

---

## Usage

### Connect to SDR Server

```powershell
# Start SDR server (from phoenix-sdr-core)
sdr_server.exe

# Start waterfall (connects to localhost:4536)
waterfall.exe
```

### Command Line Options

```
waterfall.exe [options]

Options:
  --tcp HOST:PORT    Connect to I/Q server (default: localhost:4536)
  --stdin            Read PCM audio from stdin instead of TCP
  --test             Generate synthetic 1000 Hz test pattern
  --log-csv          Enable CSV file logging (in addition to UDP)
  --help             Show this help
```

### Keyboard Controls

| Key | Action |
|-----|--------|
| `Q` / `ESC` | Quit |
| `M` | Toggle audio mute |
| `+` / `=` | Volume up |
| `-` | Volume down |
| `R` | Reload tuned parameters |
| `Space` | Toggle pause |

---

## Telemetry Protocol

UDP broadcast on port 3005 (default). CSV format with channel prefix.

### Channels

| Channel | Prefix | Description |
|---------|--------|-------------|
| CHANNEL | `CHAN` | Signal quality metrics |
| TICKS | `TICK` | Tick pulse events |
| MARKERS | `MARK` | Minute marker events |
| SYNC | `SYNC` | Sync state changes |
| BCDS | `BCDS` | BCD symbols and time |
| CONSOLE | `CONS` | Status messages |

### Example Messages

```
TICK,1000Hz,12.5ms,0.85,1734567890123
MARK,1000Hz,800ms,0.92,1734567890000
SYNC,LOCKED,3,60.5,2024-12-18T15:30:00Z
BCDS,SYM,1,1734567890500,485
```

---

## Components

### Display

| File | Description |
|------|-------------|
| `waterfall.c` | Main application, SDL rendering |
| `waterfall_dsp.c/h` | DSP processing (lowpass, DC removal) |
| `waterfall_flash.c/h` | Visual flash/marker system |
| `waterfall_audio.c/h` | Audio output (waveOut) |
| `waterfall_telemetry.c/h` | UDP telemetry broadcast |

### Detectors

| File | Description |
|------|-------------|
| `tick_detector.c/h` | 1000 Hz tick pulse detection |
| `marker_detector.c/h` | 800ms minute marker detection |
| `sync_detector.c/h` | Synchronization state machine |
| `tone_tracker.c/h` | 500/600 Hz tone tracking |
| `tick_correlator.c/h` | Tick correlation analysis |
| `marker_correlator.c/h` | Marker correlation analysis |
| `slow_marker_detector.c/h` | Slow marker detection backup |
| `channel_filters.c/h` | Sync/data channel separation |

### BCD Decoding

| File | Description |
|------|-------------|
| `bcd_envelope.c/h` | 100 Hz subcarrier envelope |
| `bcd_decoder.c/h` | BCD symbol classification |
| `bcd_time_detector.c/h` | Time-domain BCD detection |
| `bcd_freq_detector.c/h` | Frequency-domain BCD detection |
| `bcd_correlator.c/h` | Window-based BCD integration |

---

## Related Repositories

| Repository | Description |
|------------|-------------|
| [mars-suite](https://github.com/Alex-Pennington/mars-suite) | Phoenix Nest MARS Suite index |
| [phoenix-sdr-core](https://github.com/Alex-Pennington/phoenix-sdr-core) | SDR hardware interface |
| [phoenix-reference-library](https://github.com/Alex-Pennington/phoenix-reference-library) | Technical documentation |
| [phoenix-wwv](https://github.com/Alex-Pennington/phoenix-wwv) | WWV detection library |

---

## License

AGPL-3.0 — See [LICENSE](LICENSE)

---

*Phoenix Nest MARS Communications Suite*  
*KY4OLB*
