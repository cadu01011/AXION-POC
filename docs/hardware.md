# Hardware

## M5Stack ATOM Matrix v1.1 (one per zone)

| Item | Spec |
|---|---|
| SoC | ESP32-PICO-D4, dual-core LX6 @ 240 MHz |
| Radio | Wi-Fi 2.4 GHz + Bluetooth 4.2 (BLE) — single shared radio |
| Display | 5×5 WS2812C-2020 RGB LED matrix |
| Flash / SRAM | 4 MB / 520 KB |
| Input | 1 programmable button, BMI270 IMU |
| Power | USB Type-C 5 V, ~62 mA typical |

### Pin map (used by this project)

| GPIO | Function | Notes |
|---|---|---|
| G27 | WS2812C 5×5 matrix data | 800 kHz, GRB color order |
| G39 | Front button | Input-only, external pull-up, active LOW, 30 ms debounce |
| G21 / G25 | I2C SCL / SDA (BMI270 IMU) | Not used; do not reuse |
| G26 / G32 | Grove port | Free / reserved |
| G12 | IR transmitter | Not used |

### Electrical limits

- LED brightness is capped at **40/255 (~16%)** in firmware
  (`AXION_LED_BRIGHTNESS_MAX`). The M5Stack docs warn that high brightness
  damages the LEDs and the acrylic. Do not raise it.
- G39 is input-only: it cannot drive an output and has no internal pull-up.

### Single-radio constraint

Wi-Fi and BLE share one 2.4 GHz radio. The firmware mitigates contention with
IDF software coexistence (`CONFIG_ESP_COEX_SW_COEXIST_ENABLE`), a ~90% duty
passive scan, `WIFI_PS_MIN_MODEM`, and short keep-alive POSTs.

## DX-CP29 beacon (operator badge)

| Item | Value |
|---|---|
| Radio | BLE 5.1 + NFC; legacy advertising (iBeacon) |
| Adopted config | **iBeacon frame only**, advertising interval **200 ms**, TX power high |
| Identity | MAC (public, static, `addr_type=0`) is the primary key |
| Battery | Rechargeable Li-ion (USB-C), up to ~1 year |
| Range | 60–80 m |

### Important beacon note

These beacons are **multi-frame** (DeviceInfo / iBeacon / UID / URL / TLM). With
several frames enabled, only ~1 in 5 advertisements is iBeacon, so the effective
iBeacon rate collapses to ~0.2/s. In the DX-SMART app, enable **only** the
iBeacon frame (set the others to NoData) and set the interval to 200 ms. That
raises the iBeacon rate to ~2–5/s, which is what the filter needs.

## Physical layout for zone detection

- One ATOM at the center of each zone, ~2 m high, away from metal.
- **≥ 4–5 m between ATOMs** (works down to ~3 m if the beacon TX power is
  lowered). Below ~2–3 m the zones become indistinguishable.
- Validation: with a person clearly inside a zone, that zone's ATOM should hear
  the beacon **≥ 10 dB stronger** than the neighboring ATOMs.

## Power for the fleet

Each ATOM only needs 5 V USB power to run — a phone charger or power bank is
enough. Only the ATOM plugged into the PC shows a serial log; the others run
headless and report over Wi-Fi. The dashboard is where all three are observed.
