/**
 * @file rssi_filter.h
 * @brief Per-beacon sliding-window RSSI filter.
 *
 * Raw RSSI readings are written by the NimBLE scan callback (ble_scan.c) and
 * consumed by the network task (net.c) as periodic snapshots. Thread-safe.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Maximum number of beacons a snapshot can carry. */
#define RSSI_SNAPSHOT_MAX 8

/** @brief One beacon's filtered result within a snapshot. */
typedef struct {
    uint8_t  mac[6];   /**< MAC in conventional (big-endian) order. */
    float    rssi;     /**< Filtered RSSI over the window. */
    uint16_t n;        /**< Sample count kept after outlier removal. */
} rssi_entry_t;

/** @brief A point-in-time view of all live beacons. */
typedef struct {
    rssi_entry_t entries[RSSI_SNAPSHOT_MAX];
    size_t       count;
} rssi_snapshot_t;

/** @brief Reset all per-beacon buffers. */
void rssi_filter_init(void);

/**
 * @brief Record one raw RSSI reading for a beacon.
 * @param mac     Beacon MAC (6 bytes, big-endian order).
 * @param rssi    Raw RSSI in dBm.
 * @param now_ms  Monotonic timestamp in milliseconds.
 */
void rssi_filter_add(const uint8_t mac[6], int8_t rssi, int64_t now_ms);

/**
 * @brief Build the current snapshot.
 *
 * Drops outliers beyond AXION_RSSI_OUTLIER_SIGMA, applies the filter selected
 * by AXION_RSSI_FILTER (median or EMA), and expires beacons unseen for longer
 * than AXION_RSSI_WINDOW_MS.
 *
 * @param[out] out     Snapshot to fill.
 * @param      now_ms  Monotonic timestamp in milliseconds.
 */
void rssi_filter_snapshot(rssi_snapshot_t *out, int64_t now_ms);
