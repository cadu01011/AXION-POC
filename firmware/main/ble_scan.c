#include "ble_scan.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_hs_adv.h"
#include "host/util/util.h"

#include "axion_config.h"
#include "rssi_filter.h"

static const char *TAG = "SCAN";

#define WHITELIST_MAX 8
#define TRACKED_MAX   8

static const char *const s_whitelist_str[] = {
    AXION_WHITELIST_MACS
    NULL
};

static uint8_t s_whitelist[WHITELIST_MAX][6];
static size_t  s_whitelist_len;
static bool    s_discovery;

typedef struct {
    bool     in_use;
    uint8_t  mac[6];
    uint32_t n;
    int32_t  rssi_sum;
} tracked_t;

static tracked_t    s_stats[TRACKED_MAX];
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

typedef struct {
    uint32_t adv_total;
    uint32_t adv_mfg;
    uint32_t adv_ibeacon;
    uint16_t companies[8];
    uint8_t  n_companies;
} scan_diag_t;

static scan_diag_t s_diag;

static void fmt_mac(const uint8_t mac[6], char out[18])
{
    snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void fmt_uuid(const uint8_t u[16], char out[37])
{
    snprintf(out, 37,
             "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
             u[0], u[1], u[2], u[3], u[4], u[5], u[6], u[7],
             u[8], u[9], u[10], u[11], u[12], u[13], u[14], u[15]);
}

static bool parse_mac(const char *str, uint8_t out[6])
{
    unsigned v[6];
    if (sscanf(str, "%2x:%2x:%2x:%2x:%2x:%2x",
               &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6) {
        return false;
    }
    for (int i = 0; i < 6; i++) {
        out[i] = (uint8_t)v[i];
    }
    return true;
}

static bool whitelist_contains_locked(const uint8_t mac[6])
{
    for (size_t i = 0; i < s_whitelist_len; i++) {
        if (memcmp(s_whitelist[i], mac, 6) == 0) {
            return true;
        }
    }
    return false;
}

void ble_scan_set_whitelist(const char *const macs[], size_t n)
{
    uint8_t parsed[WHITELIST_MAX][6];
    size_t  count = 0;
    for (size_t i = 0; i < n && count < WHITELIST_MAX; i++) {
        if (parse_mac(macs[i], parsed[count])) {
            count++;
        } else {
            ESP_LOGW(TAG, "ignoring invalid MAC from API whitelist: %s", macs[i]);
        }
    }
    taskENTER_CRITICAL(&s_lock);
    memcpy(s_whitelist, parsed, sizeof(uint8_t[6]) * count);
    s_whitelist_len = count;
    s_discovery = (count == 0);
    taskEXIT_CRITICAL(&s_lock);
    ESP_LOGI(TAG, "whitelist updated from API: %u beacon(s)%s",
             (unsigned)count, count == 0 ? " (DISCOVERY)" : "");
}

static const uint8_t *find_mfg_data(const uint8_t *data, uint8_t len, uint8_t *out_len)
{
    uint8_t i = 0;
    while (i + 1 < len) {
        uint8_t field_len = data[i];
        if (field_len == 0 || i + 1 + field_len > len) {
            break;
        }
        if (data[i + 1] == 0xFF) {
            *out_len = field_len - 1;
            return &data[i + 2];
        }
        i += 1 + field_len;
    }
    return NULL;
}

static void stats_add(const uint8_t mac[6], int8_t rssi)
{
    taskENTER_CRITICAL(&s_lock);
    int free_slot = -1;
    for (int i = 0; i < TRACKED_MAX; i++) {
        if (s_stats[i].in_use && memcmp(s_stats[i].mac, mac, 6) == 0) {
            s_stats[i].n++;
            s_stats[i].rssi_sum += rssi;
            taskEXIT_CRITICAL(&s_lock);
            return;
        }
        if (!s_stats[i].in_use && free_slot < 0) {
            free_slot = i;
        }
    }
    if (free_slot >= 0) {
        s_stats[free_slot].in_use = true;
        memcpy(s_stats[free_slot].mac, mac, 6);
        s_stats[free_slot].n = 1;
        s_stats[free_slot].rssi_sum = rssi;
    }
    taskEXIT_CRITICAL(&s_lock);
}

static int gap_event_cb(struct ble_gap_event *event, void *arg)
{
    if (event->type != BLE_GAP_EVENT_DISC) {
        return 0;
    }
    const struct ble_gap_disc_desc *d = &event->disc;

    taskENTER_CRITICAL(&s_lock);
    s_diag.adv_total++;
    taskEXIT_CRITICAL(&s_lock);

    uint8_t mfg_len = 0;
    const uint8_t *m = find_mfg_data(d->data, d->length_data, &mfg_len);
    if (m == NULL || mfg_len < 2) {
        return 0;
    }

    uint16_t company = (uint16_t)m[0] | ((uint16_t)m[1] << 8);
    taskENTER_CRITICAL(&s_lock);
    s_diag.adv_mfg++;
    bool seen = false;
    for (int i = 0; i < s_diag.n_companies; i++) {
        if (s_diag.companies[i] == company) {
            seen = true;
            break;
        }
    }
    if (!seen && s_diag.n_companies < 8) {
        s_diag.companies[s_diag.n_companies++] = company;
    }
    taskEXIT_CRITICAL(&s_lock);

    if (mfg_len < 25 || m[0] != 0x4C || m[1] != 0x00 || m[2] != 0x02 || m[3] != 0x15) {
        return 0;
    }

    taskENTER_CRITICAL(&s_lock);
    s_diag.adv_ibeacon++;
    taskEXIT_CRITICAL(&s_lock);

    uint8_t mac[6];
    for (int i = 0; i < 6; i++) {
        mac[i] = d->addr.val[5 - i];
    }

    taskENTER_CRITICAL(&s_lock);
    bool discovery = s_discovery;
    bool inlist = whitelist_contains_locked(mac);
    taskEXIT_CRITICAL(&s_lock);
    if (!discovery && !inlist) {
        return 0;
    }

    uint16_t major = ((uint16_t)m[20] << 8) | m[21];
    uint16_t minor = ((uint16_t)m[22] << 8) | m[23];

    char mac_s[18];
    fmt_mac(mac, mac_s);

    if (discovery) {
        char uuid_s[37];
        fmt_uuid(&m[4], uuid_s);
        ESP_LOGI(TAG, "DISCOVERY mac=%s addr_type=%d rssi=%d uuid=%s major=%u minor=%u",
                 mac_s, d->addr.type, d->rssi, uuid_s, major, minor);
    } else {
        ESP_LOGI(TAG, "mac=%s rssi=%d major=%u minor=%u", mac_s, d->rssi, major, minor);
    }

    stats_add(mac, d->rssi);
    rssi_filter_add(mac, d->rssi, esp_timer_get_time() / 1000);
    return 0;
}

static void start_scan(void)
{
    struct ble_gap_disc_params params = {
        .itvl              = AXION_BLE_SCAN_INTERVAL,
        .window            = AXION_BLE_SCAN_WINDOW,
        .filter_policy     = 0,
        .limited           = 0,
        .passive           = 1,
        .filter_duplicates = 0,
    };
    int rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, BLE_HS_FOREVER, &params,
                          gap_event_cb, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_disc failed rc=%d", rc);
    } else {
        ESP_LOGI(TAG, "continuous passive scan: interval=100ms window=90ms duplicates=off");
    }
}

static void on_reset(int reason)
{
    ESP_LOGW(TAG, "host reset, reason=%d", reason);
}

static void on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_hs_util_ensure_addr rc=%d", rc);
        return;
    }
    start_scan();
}

static void host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static void stats_task(void *arg)
{
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(AXION_SCAN_STATS_PERIOD_MS));

        tracked_t   snap[TRACKED_MAX];
        scan_diag_t diag;
        taskENTER_CRITICAL(&s_lock);
        memcpy(snap, s_stats, sizeof(snap));
        diag = s_diag;
        memset(&s_diag, 0, sizeof(s_diag));
        for (int i = 0; i < TRACKED_MAX; i++) {
            s_stats[i].n = 0;
            s_stats[i].rssi_sum = 0;
        }
        taskEXIT_CRITICAL(&s_lock);

        char ids[48] = "";
        int  pos = 0;
        for (int i = 0; i < diag.n_companies && pos < (int)sizeof(ids) - 6; i++) {
            pos += snprintf(&ids[pos], sizeof(ids) - pos, "%s%04X",
                            i ? "," : "", diag.companies[i]);
        }
        ESP_LOGI(TAG, "diag 10s: adv_total=%" PRIu32 " com_mfg=%" PRIu32
                 " ibeacon=%" PRIu32 " mfg_ids=[%s] heap=%" PRIu32,
                 diag.adv_total, diag.adv_mfg, diag.adv_ibeacon, ids,
                 esp_get_free_heap_size());

        bool any = false;
        for (int i = 0; i < TRACKED_MAX; i++) {
            if (!snap[i].in_use || snap[i].n == 0) {
                continue;
            }
            any = true;
            char mac_s[18];
            fmt_mac(snap[i].mac, mac_s);
            float rate = (float)snap[i].n * 1000.0f / (float)AXION_SCAN_STATS_PERIOD_MS;
            float avg  = (float)snap[i].rssi_sum / (float)snap[i].n;
            ESP_LOGI(TAG, "stats %us mac=%s n=%" PRIu32 " rate=%.1f/s rssi_med=%.1f",
                     AXION_SCAN_STATS_PERIOD_MS / 1000, mac_s, snap[i].n, rate, avg);
        }
        if (!any) {
            if (diag.adv_total == 0) {
                ESP_LOGW(TAG, "no BLE packet from ANY device in %u s "
                              "-> radio/scan problem on the ATOM",
                         AXION_SCAN_STATS_PERIOD_MS / 1000);
            } else {
                ESP_LOGW(TAG, "scan OK (%" PRIu32 " advs from other devices) but "
                              "no %s in %u s -> beacon problem: disconnect the "
                              "DX-SMART app, check the iBeacon frame is enabled/saved",
                         diag.adv_total,
                         s_discovery ? "iBeacon" : "whitelisted beacon",
                         AXION_SCAN_STATS_PERIOD_MS / 1000);
            }
        }
    }
}

esp_err_t ble_scan_init(void)
{
    for (size_t i = 0; s_whitelist_str[i] != NULL; i++) {
        if (i >= WHITELIST_MAX) {
            ESP_LOGE(TAG, "whitelist exceeds %d entries", WHITELIST_MAX);
            return ESP_ERR_INVALID_ARG;
        }
        if (!parse_mac(s_whitelist_str[i], s_whitelist[i])) {
            ESP_LOGE(TAG, "invalid MAC in whitelist: %s", s_whitelist_str[i]);
            return ESP_ERR_INVALID_ARG;
        }
        s_whitelist_len++;
    }
    s_discovery = (s_whitelist_len == 0);
    if (s_discovery) {
        ESP_LOGW(TAG, "empty whitelist -> DISCOVERY mode: every iBeacon is "
                      "logged. Fill AXION_WHITELIST_MACS in axion_config.h or "
                      "register beacons via the API.");
    } else {
        ESP_LOGI(TAG, "whitelist active with %u beacon(s)", (unsigned)s_whitelist_len);
    }

    ESP_ERROR_CHECK(nimble_port_init());
    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb  = on_sync;
    nimble_port_freertos_init(host_task);

    if (xTaskCreatePinnedToCore(stats_task, "scan_stats", 4096, NULL, 2, NULL, 1) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
