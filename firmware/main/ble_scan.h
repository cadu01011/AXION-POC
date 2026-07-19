/**
 * @file ble_scan.h
 * @brief Passive BLE observer that detects iBeacon advertisements.
 *
 * Accepted iBeacon frames are logged under the "SCAN" tag, counted in
 * periodic statistics, and forwarded to the RSSI filter. An empty whitelist
 * puts the scanner in DISCOVERY mode, where every iBeacon is logged.
 */
#pragma once

#include <stddef.h>
#include "esp_err.h"

/**
 * @brief Initialize NimBLE (observer role) and start the continuous
 *        passive scan, plus the periodic statistics task.
 * @return ESP_OK on success, or an error if a MAC in the compile-time
 *         whitelist is invalid or a task cannot be created.
 */
esp_err_t ble_scan_init(void);

/**
 * @brief Replace the active whitelist at runtime (MAC list from the API).
 *
 * Thread-safe. Invalid MACs are skipped. Passing n == 0 returns the scanner
 * to DISCOVERY mode (every iBeacon logged).
 *
 * @param macs  Array of MAC strings ("AA:BB:CC:DD:EE:FF").
 * @param n     Number of entries in @p macs.
 */
void ble_scan_set_whitelist(const char *const macs[], size_t n);
