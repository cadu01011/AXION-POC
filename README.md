# AXION — BLE Zone Localization

A proof of concept that identifies which zone (A, B or C) each operator is in,
using BLE beacons (badges) and three M5Stack ATOM Matrix devices, with the zone
decision on a server and a live web dashboard. BrSupply technical challenge (see
`AXION_POC_Desafio_v2.pdf`).

## How it works

```
[BLE beacon = person] --iBeacon--> [ATOM per zone: scan + RSSI filter] --HTTP--> [Server: API + zone engine + SQLite]
                                                                                        |--> [Dashboard (WebSocket)]
```

Every ATOM hears every beacon, at a different signal strength. The three ATOMs
report the filtered RSSI to the server, which assigns each person to the zone
whose ATOM hears them strongest (argmax), with hysteresis, a presence floor and
timeouts. A single ATOM cannot decide a zone — the comparison across three, on
the server, does.

## Repository layout

```
axion/
├── firmware/     ESP-IDF (C) — BLE scan, RSSI filter, Wi-Fi/HTTP, display
├── server/       FastAPI + SQLite — API, zone engine, dashboard
├── tools/        API mock and reading simulator (test without hardware)
├── docs/         documentation (English) + developer guide (pt-BR)
└── PLANO_IMPLEMENTACAO.md   phased spec (pt-BR)
```

## Quick start

```
# server
cd server && python -m venv .venv && .venv\Scripts\activate && pip install -r requirements.txt
python -m uvicorn app.main:app --host 0.0.0.0 --port 8000
# dashboard at http://localhost:8000/ , registry at /beacons
```

Full build/flash steps: [docs/build-and-run.md](docs/build-and-run.md).

## Test without hardware

```
python server/tests/test_zones.py                       # zone algorithm unit tests
python tools/simulator.py http://127.0.0.1:8000 --seed  # end-to-end against the API
```

## Documentation

| Doc | Contents |
|---|---|
| [docs/architecture.md](docs/architecture.md) | System design, signal path, decisions |
| [docs/hardware.md](docs/hardware.md) | ATOM pinout, beacon setup, physical layout |
| [docs/build-and-run.md](docs/build-and-run.md) | Build the firmware, run the server, flash 3 ATOMs |
| [docs/api-reference.md](docs/api-reference.md) | Endpoints and JSON contracts |
| [docs/configuration.md](docs/configuration.md) | Every tunable constant |
| [docs/testing.md](docs/testing.md) | Unit, simulator, mock and on-device tests |
| [docs/CODING_STANDARDS.md](docs/CODING_STANDARDS.md) | Naming and documentation conventions |
| [PLANO_IMPLEMENTACAO.md](PLANO_IMPLEMENTACAO.md) | Phased implementation plan (pt-BR) |
