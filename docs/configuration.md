# Configuration reference

All tunables live in three places. Change a value in the matching file.

## Firmware — `firmware/main/axion_config.h`

| Constant | Default | Meaning |
|---|---|---|
| `AXION_DEBUG` | 1 | Emit `[DBG]` lines as INFO logs (0 for production) |
| `AXION_LED_GPIO` | 27 | LED matrix data pin |
| `AXION_LED_COUNT` | 25 | LEDs in the matrix |
| `AXION_LED_BRIGHTNESS_MAX` | 40 | Global brightness cap 0-255 (do not raise) |
| `AXION_BTN_GPIO` | 39 | Front button pin |
| `AXION_BTN_DEBOUNCE_MS` | 30 | Button debounce |
| `AXION_DISPLAY_LETTER_MS` | 2000 | Zone-letter phase duration |
| `AXION_DISPLAY_COUNT_MS` | 2000 | Operator-count phase duration |
| `AXION_BLE_SCAN_INTERVAL` | 160 | Scan interval (×0.625 ms = 100 ms) |
| `AXION_BLE_SCAN_WINDOW` | 144 | Scan window (×0.625 ms = 90 ms) |
| `AXION_SCAN_STATS_PERIOD_MS` | 10000 | Diagnostic stats period |
| `AXION_RSSI_WINDOW_MS` | 3000 | Filter sliding window |
| `AXION_RSSI_EMA_ALPHA` | 0.30 | EMA smoothing factor |
| `AXION_RSSI_OUTLIER_SIGMA` | 2.0 | Outlier rejection threshold |
| `AXION_SNAPSHOT_PERIOD_MS` | 2000 | Snapshot / POST period |
| `AXION_RSSI_FILTER` | MEDIAN | Active filter (MEDIAN or EMA) |
| `AXION_HTTP_TIMEOUT_MS` | 1500 | HTTP client timeout |
| `AXION_OFFLINE_AFTER_FAILS` | 3 | Failed POSTs before OFFLINE |
| `AXION_WIFI_BACKOFF_MIN_MS` | 1000 | Initial reconnect backoff |
| `AXION_WIFI_BACKOFF_MAX_MS` | 30000 | Max reconnect backoff |
| `AXION_LOCAL_COUNT_ENTER_DBM` | -82 | Local count: enter threshold |
| `AXION_LOCAL_COUNT_EXIT_DBM` | -88 | Local count: leave threshold |
| `AXION_WHITELIST_REFRESH_S` | 60 | Periodic whitelist refresh |
| `AXION_WHITELIST_API_MAX` | 8 | Max MACs from API whitelist |

## Firmware — per device (`sdkconfig.defaults` + presets, or menuconfig)

| Kconfig | Meaning |
|---|---|
| `CONFIG_AXION_ATOM_ID` | `atom-a` / `atom-b` / `atom-c` (must match the server seed) |
| `CONFIG_AXION_ZONE` | `A` / `B` / `C` |
| `CONFIG_AXION_WIFI_SSID` | 2.4 GHz SSID (empty = Wi-Fi disabled) |
| `CONFIG_AXION_WIFI_PASSWORD` | Wi-Fi password |
| `CONFIG_AXION_API_URL` | Server base URL, e.g. `http://192.168.0.42:8000` |

The presets `sdkconfig.atom-a/b/c` set only the identity; Wi-Fi and URL are
shared in `sdkconfig.defaults`.

## Server — `server/app/config.py`

| Constant | Default | Meaning |
|---|---|---|
| `HYSTERESIS_DB` | 5.0 | Margin (dB) a new zone must win by |
| `HYSTERESIS_HOLD_S` | 3.0 | ... held continuously for this long |
| `PRESENCE_TIMEOUT_S` | 15.0 | Beacon drops from the count after this |
| `PRESENCE_RSSI_MIN` | -88.0 | Weaker than this everywhere = absent |
| `ATOM_OFFLINE_S` | 10.0 | ATOM shown OFFLINE after this without a POST |
| `ZONES` | A, B, C | Zone list |
| `ATOMS` | atom-a→A, … | ATOM id → zone map |
| `RECOMPUTE_PERIOD_S` | 1.0 | Timeout/offline sweep timer |
| `DB_PATH` | `axion.db` | SQLite path (env `AXION_DB` overrides) |

These server thresholds and the firmware filter choice are the knobs tuned in
phase 7 (calibration).
