# Build and run

## 1. Server (PC)

Requires Python 3.10+.

```
cd server
python -m venv .venv
.venv\Scripts\activate            # Windows  (source .venv/bin/activate on Linux/macOS)
pip install -r requirements.txt
python -m uvicorn app.main:app --host 0.0.0.0 --port 8000
```

- Dashboard: http://localhost:8000/
- Beacon registry: http://localhost:8000/beacons
- The server creates `axion.db` (SQLite) on first run, seeded with the three
  ATOMs and three example beacons.

Use `--host 0.0.0.0` (not `127.0.0.1`) so the ATOMs can reach it. Find the PC
IP with `ipconfig` and make sure the PC and ATOMs share the same network.

## 2. Firmware (each ATOM)

Requires ESP-IDF v5.5+ (tested on v6.0.x). Fill in your Wi-Fi and the server IP
once in `firmware/sdkconfig.defaults`:

```
CONFIG_AXION_WIFI_SSID="YOUR_WIFI_2G"
CONFIG_AXION_WIFI_PASSWORD="YOUR_PASSWORD"
CONFIG_AXION_API_URL="http://<PC-IP>:8000"
```

Flash the three ATOMs, one at a time, each with its own identity preset:

```
cd firmware
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.atom-a" -p COM5 -b 115200 flash monitor   # Zone A
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.atom-b" -p COM6 -b 115200 flash            # Zone B
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.atom-c" -p COM7 -b 115200 flash            # Zone C
```

Alternatively use `idf.py menuconfig` (menu **AXION**) and change only the ATOM
ID and Zone between flashes; Wi-Fi and URL persist in `sdkconfig`.

Delete `sdkconfig` before switching presets so it is regenerated:
`Remove-Item sdkconfig -Force` (PowerShell) or `rm -f sdkconfig`.

### Wi-Fi off (BLE/filter only)

Leave `CONFIG_AXION_WIFI_SSID` empty to disable Wi-Fi. The firmware then only
scans, filters, and shows an approximate local count on the display — useful for
bench testing without the server.

## 3. Beacons (DX-SMART app)

Each beacon must have **only the iBeacon frame enabled** (others set to NoData),
**200 ms** interval, and high TX power. Give each beacon a distinct Major (1–5).
Then register each beacon's MAC on the `/beacons` page. See `hardware.md`.

## 4. Confirm it works

- Serial log of the plugged ATOM: `Wi-Fi connected` → `ONLINE state`.
- Dashboard: each ATOM card turns ONLINE; the operator count appears in its
  zone and migrates as a beacon moves between zones.

## Finding the beacon MACs

With the whitelist empty (remove all beacons from `/beacons`), the firmware runs
in DISCOVERY mode and logs every iBeacon:

```
SCAN: DISCOVERY mac=48:87:2D:9D:8F:89 addr_type=0 rssi=-75 uuid=... major=2 minor=1
```

Read the `mac=` field, then register it. Powering one beacon at a time tells you
which MAC belongs to which operator (match by Major). The phone app nRF Connect
also shows device MACs.
