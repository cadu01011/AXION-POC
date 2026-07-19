/**
 * @file net.h
 * @brief Wi-Fi station and HTTP reporting to the AXION API.
 *
 * A background task builds an RSSI snapshot every AXION_SNAPSHOT_PERIOD_MS and
 * POSTs it to /api/readings, fetching the beacon whitelist from the API. If
 * CONFIG_AXION_WIFI_SSID is empty, Wi-Fi is disabled and the task only logs
 * snapshots (so BLE scan and RSSI filter can be tested without a network).
 */
#pragma once

#include <stdbool.h>
#include "esp_err.h"

/** @brief Display-facing connection state (drives the display state machine). */
typedef enum {
    NET_DISABLED,    /**< Wi-Fi off (empty SSID): local count only. */
    NET_CONNECTING,  /**< Wi-Fi enabled, not yet connected. */
    NET_ONLINE,      /**< Connected and API responding. */
    NET_OFFLINE,     /**< AXION_OFFLINE_AFTER_FAILS consecutive failed POSTs. */
} net_state_t;

/**
 * @brief Start Wi-Fi (if configured) and the reporting task.
 * @return ESP_OK on success, ESP_ERR_NO_MEM if the task cannot be created.
 */
esp_err_t net_init(void);

/** @brief Current connection state. */
net_state_t net_state(void);

/**
 * @brief Operator count for this ATOM's zone from the last API response.
 * @return Count, or -1 if the API has not answered yet.
 */
int net_zone_count(void);

/**
 * @brief Local operator count (no-API preview).
 *
 * Beacons in the current filter window with hysteresis on RSSI
 * (AXION_LOCAL_COUNT_ENTER_DBM / EXIT), used by the display when there is
 * no API answer.
 * @return Local beacon count.
 */
int net_local_count(void);
