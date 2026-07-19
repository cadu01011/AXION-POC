# Architecture

## Overview

AXION answers one question: **which zone (A, B or C) is each operator in?**
Operators carry BLE beacons; one ATOM Matrix per zone listens for them; a
server decides the zone and a web dashboard shows it live.

```
 ┌─────────────┐   iBeacon adv    ┌──────────────────────────┐   HTTP/JSON   ┌───────────────┐
 │  DX-CP29    │ ───────────────► │  ATOM Matrix (per zone)  │ ────────────► │  Server       │
 │  beacon     │                  │  scan → RSSI filter      │               │  API + SQLite │
 │  = operator │                  │  → HTTP client → display │ ◄──────────── │  zone engine  │
 └─────────────┘                  └──────────────────────────┘  zone_count   └───────┬───────┘
                                                                                      │ WebSocket
                                                                              ┌───────▼───────┐
                                                                              │  Dashboard    │
                                                                              └───────────────┘
```

## Why the zone decision is on the server

Every ATOM hears every beacon, only at a different signal strength (RSSI).
A single ATOM cannot know whether another ATOM hears the same beacon more
strongly, so if each ATOM decided locally it could double-count one operator
across zones. The server collects the three ATOMs' filtered RSSI per beacon and
assigns each beacon to exactly one zone: the ATOM that hears it strongest
(argmax). This is the standard approach for zone-level (proximity) positioning.

## Signal path, stage by stage

1. **Beacon** advertises an iBeacon frame (~200 ms interval). Its MAC is the
   operator's identity.
2. **BLE scan** (`ble_scan.c`) runs a continuous passive scan, parses iBeacon
   frames, filters by the whitelist, and forwards `(mac, rssi)` to the filter.
3. **RSSI filter** (`rssi_filter.c`) keeps a 3 s sliding window per beacon,
   drops outliers, and produces one stable value per beacon (median by
   default). RSSI swings ±10 dB raw; the filter brings it to ±3 dB.
4. **Network** (`net.c`) builds a snapshot every 2 s and POSTs it to
   `/api/readings`. The response carries this zone's operator count and the
   whitelist version.
5. **Server** (`app/zones.py` + `app/main.py`) runs the zone engine: argmax
   across ATOMs + hysteresis + presence floor + timeout. It stores raw readings
   (calibration dataset) and pushes state to the dashboard over WebSocket.
6. **Display** (`main.c`) cycles the zone letter and the operator count, and
   shows connection state (connecting, offline, online).

## Robustness layers

- **Two-stage filtering:** median/EMA on the ATOM (removes jitter, shrinks the
  payload) + temporal hysteresis on the server (removes zone flapping).
- **Hysteresis:** a beacon only switches zone if the candidate zone beats the
  current one by ≥ 5 dB continuously for ≥ 3 s.
- **Presence floor:** a beacon weaker than −88 dBm on every ATOM counts as
  absent.
- **Timeouts:** a beacon with no reading for 15 s drops out; an ATOM with no
  POST for 10 s shows OFFLINE.
- **Offline handling:** the ATOM enters an OFFLINE display state after three
  failed POSTs and recovers on the first success.

## Components

| Component | Tech | Location |
|---|---|---|
| Firmware | ESP-IDF (C), NimBLE observer | `firmware/` |
| API + zone engine | Python, FastAPI, SQLite (WAL) | `server/app/` |
| Dashboard + registry | HTML/CSS/vanilla JS, WebSocket | `server/static/` |
| Test tools | Python stdlib | `tools/` |

## Alternatives considered

- **Trilateration** (estimate x,y): needs a calibrated propagation model and
  good anchor geometry; indoor RSSI error is several meters. Overkill for a
  discrete A/B/C question.
- **Fingerprinting** (RSSI map per point): more accurate, but needs a survey
  campaign and re-calibration on any layout change. Listed as a next step.
- **Edge decision** (each ATOM decides): causes double-counting. Rejected.

See `docs/GUIA_DO_DESENVOLVEDOR.md` for a line-by-line walkthrough (pt-BR).
