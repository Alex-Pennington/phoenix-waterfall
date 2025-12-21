# Telemetry Protocol

UDP broadcast telemetry for remote monitoring of waterfall detection events.

## Connection

- **Protocol:** UDP (broadcast)
- **Port:** 3005 (default)
- **Format:** CSV with channel prefix

## Message Format

```
PREFIX,field1,field2,...\n
```

Each message starts with a 4-character channel prefix, followed by comma-separated values.

---

## Channels

### CHAN — Channel Quality

Signal quality metrics updated once per display frame.

```
CHAN,carrier_db,snr_db,noise_db,timestamp_ms
```

| Field | Type | Description |
|-------|------|-------------|
| carrier_db | float | Carrier power in dB |
| snr_db | float | Signal-to-noise ratio |
| noise_db | float | Noise floor in dB |
| timestamp_ms | uint64 | Unix timestamp (ms) |

### TICK — Tick Events

1000 Hz tick pulse detections.

```
TICK,freq_hz,duration_ms,confidence,timestamp_ms
```

| Field | Type | Description |
|-------|------|-------------|
| freq_hz | int | Detected frequency (nominally 1000) |
| duration_ms | float | Pulse duration |
| confidence | float | Detection confidence (0-1) |
| timestamp_ms | uint64 | Event timestamp |

### MARK — Marker Events

800ms minute marker detections.

```
MARK,freq_hz,duration_ms,confidence,timestamp_ms
```

| Field | Type | Description |
|-------|------|-------------|
| freq_hz | int | Detected frequency |
| duration_ms | float | Marker duration |
| confidence | float | Detection confidence (0-1) |
| timestamp_ms | uint64 | Event timestamp |

### SYNC — Sync State

Synchronization state changes.

```
SYNC,state,confidence,elapsed_sec,time_iso
```

| Field | Type | Description |
|-------|------|-------------|
| state | string | SEARCHING, ACQUIRING, LOCKED, RECOVERING |
| confidence | int | Confidence level (0-4) |
| elapsed_sec | float | Seconds since last marker |
| time_iso | string | Decoded UTC time (when LOCKED) |

### BCDS — BCD Symbols

BCD time code symbols and decoded time.

```
BCDS,SYM,value,timestamp_ms,duration_ms
BCDS,TIME,HH:MM:SS,day_of_year,year
```

| Field | Type | Description |
|-------|------|-------------|
| SYM/TIME | string | Message type |
| value | int | Symbol value (0, 1, or P for marker) |
| timestamp_ms | uint64 | Symbol timestamp |
| duration_ms | float | Pulse duration |

### CONS — Console Messages

Status and debug messages.

```
CONS,message_text
```

---

## Listening Example (Python)

```python
import socket

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
sock.bind(('', 3005))

while True:
    data, addr = sock.recvfrom(512)
    line = data.decode().strip()
    prefix = line[:4]
    payload = line[5:]
    
    if prefix == 'TICK':
        freq, dur, conf, ts = payload.split(',')
        print(f"Tick: {dur}ms @ {conf} confidence")
    elif prefix == 'SYNC':
        state, conf, elapsed, time_str = payload.split(',')
        print(f"Sync: {state} ({conf}) - {time_str}")
```

---

## Channel Enable/Disable

Channels can be enabled or disabled at runtime via the control interface or by modifying the enabled channel mask in the source.

Default enabled channels:
- TICKS
- MARKERS
- SYNC
- BCDS
- CONSOLE
