# Testing

The system can be tested at three levels: pure logic (no hardware), the full
HTTP path (no ATOMs), and on-device.

## 1. Zone algorithm — unit tests (no hardware)

Runs the argmax + hysteresis + timeout + presence-floor + no-double-count logic
in isolation. No dependencies.

```
python server/tests/test_zones.py
```

Expected: `Ran 10 tests ... OK`.

## 2. Full API path — simulator (no ATOMs)

With the server running, `tools/simulator.py` imitates the three ATOMs and walks
through scenarios (enter A, seen by two ATOMs, cross to B, disappear).

```
# terminal 1
cd server && python -m uvicorn app.main:app --host 127.0.0.1 --port 8000

# terminal 2
python tools/simulator.py http://127.0.0.1:8000 --seed
```

Expected: each step prints `[OK]` and the run ends with `=== RESULT: ALL OK ===`.

## 3. Firmware without the server — the mock

`tools/mock_api.py` accepts the ATOM POSTs, prints them, and flags delivery gaps
or lost sequence numbers on its own.

```
python tools/mock_api.py            # default port 8000
```

Point `CONFIG_AXION_API_URL` at the machine running the mock. Press Ctrl+C for a
summary; zero warnings over 10 minutes is a pass for delivery.

## 4. On-device checks (serial log)

Key log lines and what they mean:

| Line | Meaning |
|---|---|
| `SCAN: DISCOVERY mac=... rssi=...` | iBeacon seen in discovery mode |
| `SCAN: stats 10s mac=... rate=X/s` | Per-beacon read rate (target ≥ 3/s) |
| `SCAN: diag 10s: adv_total=... ibeacon=... heap=...` | Radio health and free heap |
| `FILT: snapshot: mac=rssi(n=...)` | Filtered value per beacon |
| `NET: Wi-Fi connected` / `ONLINE state` | Network up |
| `NET: OFFLINE state (3 ...)` | Three POSTs failed in a row |

Health signals:

- **Read rate:** `rate` in `stats 10s` should be ~2–5/s per beacon. If it is
  ~0.2/s, the beacon still has extra frames enabled (see `hardware.md`).
- **Filter stability:** a still beacon's filtered RSSI should vary ≤ ±3 dB.
- **Coexistence:** `adv_total` with Wi-Fi on should stay ≥ 70% of the value
  with Wi-Fi off.
- **Stability:** over an hour the boot banner appears once (no reboots) and
  `heap` does not trend down.

## 5. Zone accuracy (phase 7, on-device)

With three ATOMs placed per `hardware.md`, put a person in a known zone and
confirm the dashboard assigns them correctly; walk between zones and confirm the
count migrates within ~10 s; stand on a boundary and confirm it does not
oscillate. Build a confusion matrix (real zone × detected zone) from the
`readings` table to quantify accuracy and to tune the `config.py` thresholds.
