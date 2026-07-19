/**
 * @file axion_config.h
 * @brief Compile-time configuration constants shared across the firmware.
 *
 * Single source of truth for tunable parameters (GPIOs, timings, thresholds).
 * Mirrors section 3.1 of PLANO_IMPLEMENTACAO.md; change a value here first,
 * then in the plan. Grouped by subsystem.
 */
#pragma once

#include "esp_log.h"

/**
 * @def AXION_DEBUG
 * @brief Set to 1 to emit AXION_DBG() lines as INFO logs (visible without
 *        touching the menuconfig log level). Set to 0 for production.
 */
#define AXION_DEBUG 1
#if AXION_DEBUG
#define AXION_DBG(tag, fmt, ...) ESP_LOGI(tag, "[DBG] " fmt, ##__VA_ARGS__)
#else
#define AXION_DBG(tag, fmt, ...) do {} while (0)
#endif

/* ---- Hardware (ATOM Matrix v1.1) ---- */
#define AXION_LED_GPIO            27    /**< WS2812C 5x5 matrix data line. */
#define AXION_LED_COUNT           25    /**< Number of LEDs in the matrix. */
#define AXION_LED_BRIGHTNESS_MAX  40    /**< Global brightness cap 0-255 (electrical limit; do not raise). */
#define AXION_BTN_GPIO            39    /**< Front button (input-only, external pull-up, active LOW). */
#define AXION_BTN_DEBOUNCE_MS     30    /**< Button debounce window. */

/* ---- Display ---- */
#define AXION_LED_SWEEP_STEP_MS   80    /**< Per-LED delay of the boot mapping sweep. */
#define AXION_DISPLAY_LETTER_MS   2000  /**< Zone-letter phase duration of the display cycle. */
#define AXION_DISPLAY_COUNT_MS    2000  /**< Operator-count phase duration of the display cycle. */

/* ---- BLE scan (units of 0.625 ms) ---- */
#define AXION_BLE_SCAN_INTERVAL   160   /**< Scan interval (160 * 0.625 ms = 100 ms). */
#define AXION_BLE_SCAN_WINDOW     144   /**< Scan window (144 * 0.625 ms = 90 ms, ~90% duty). */

/* ---- Scan statistics logging ---- */
#define AXION_SCAN_STATS_PERIOD_MS 10000 /**< Period of the diagnostic stats log. */

/* ---- RSSI filter ---- */
#define AXION_RSSI_WINDOW_MS      3000   /**< Sliding window length per beacon. */
#define AXION_RSSI_EMA_ALPHA      0.30f  /**< EMA smoothing factor (alternative filter). */
#define AXION_RSSI_OUTLIER_SIGMA  2.0f   /**< Outliers beyond this many sigmas are dropped. */
#define AXION_SNAPSHOT_PERIOD_MS  2000   /**< Period of the snapshot build / POST. */

#define AXION_RSSI_FILTER_MEDIAN  0      /**< Median filter selector value. */
#define AXION_RSSI_FILTER_EMA     1      /**< EMA filter selector value. */
#define AXION_RSSI_FILTER         AXION_RSSI_FILTER_MEDIAN /**< Active filter. */

/* ---- Network ---- */
#define AXION_HTTP_TIMEOUT_MS       1500  /**< HTTP client timeout. */
#define AXION_OFFLINE_AFTER_FAILS   3     /**< Consecutive failed POSTs before OFFLINE state. */
#define AXION_WIFI_BACKOFF_MIN_MS   1000  /**< Initial Wi-Fi reconnect backoff. */
#define AXION_WIFI_BACKOFF_MAX_MS   30000 /**< Maximum Wi-Fi reconnect backoff. */

/**
 * @brief Hysteresis thresholds for the local (no-API) display count.
 *
 * A beacon enters the local count at RSSI >= ENTER and only leaves at
 * RSSI < EXIT, preventing flicker when a beacon sits on the threshold.
 * The authoritative zone decision (using all three ATOMs) lives on the
 * server; this local count is only the offline preview.
 */
#define AXION_LOCAL_COUNT_ENTER_DBM  -82
#define AXION_LOCAL_COUNT_EXIT_DBM   -88

/* ---- API whitelist ---- */
#define AXION_WHITELIST_REFRESH_S   60   /**< Periodic whitelist refresh (besides on version change). */
#define AXION_WHITELIST_API_MAX     8    /**< Maximum MACs accepted from the API whitelist. */

/**
 * @def AXION_WHITELIST_MACS
 * @brief Compile-time whitelist fallback (used until the API supplies one).
 *
 * Empty list => DISCOVERY mode: every received iBeacon is logged so real
 * beacon MACs can be found. In normal operation the whitelist comes from
 * GET /api/beacons/active.
 */
#define AXION_WHITELIST_MACS \
    /* "C3:FA:7B:12:5E:01", */ \
    /* "9A:12:88:04:AA:02", */ \
    /* "1B:55:20:F0:31:03", */
