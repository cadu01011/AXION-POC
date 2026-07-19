#include "net.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "sdkconfig.h"

#include "axion_config.h"
#include "ble_scan.h"
#include "rssi_filter.h"

static const char *TAG      = "NET";
static const char *TAG_FILT = "FILT";

static bool          s_wifi_enabled;
static volatile bool s_connected;
static volatile bool s_ever_connected;
static volatile bool s_offline = true;
static volatile int  s_zone_count = -1;
static volatile int  s_local_count;
static uint32_t      s_seq;
static int           s_fail_count;
static int64_t       s_last_attempt_ms;
static uint32_t      s_backoff_ms = AXION_WIFI_BACKOFF_MIN_MS;
static char          s_url_readings[192];
static char          s_url_beacons[192];
static int           s_whitelist_ver = -1;
static int64_t       s_last_wl_refresh_ms;
static esp_http_client_handle_t s_client;

static uint8_t s_lc_mac[RSSI_SNAPSHOT_MAX][6];
static bool    s_lc_on[RSSI_SNAPSHOT_MAX];
static size_t  s_lc_n;

static void fmt_mac(const uint8_t mac[6], char out[18])
{
    snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_connected) {
            ESP_LOGW(TAG, "Wi-Fi disconnected");
        }
        s_connected = false;
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        s_connected = true;
        s_ever_connected = true;
        s_backoff_ms = AXION_WIFI_BACKOFF_MIN_MS;
        ESP_LOGI(TAG, "Wi-Fi connected");
    }
}

static void try_reconnect(int64_t now_ms)
{
    if (now_ms - s_last_attempt_ms < (int64_t)s_backoff_ms) {
        return;
    }
    s_last_attempt_ms = now_ms;
    ESP_LOGI(TAG, "reconnecting Wi-Fi (next backoff %" PRIu32 " ms)", s_backoff_ms);
    esp_wifi_connect();
    s_backoff_ms = (s_backoff_ms * 2 > AXION_WIFI_BACKOFF_MAX_MS)
                       ? AXION_WIFI_BACKOFF_MAX_MS
                       : s_backoff_ms * 2;
}

static void mark_fail(void)
{
    if (s_fail_count < AXION_OFFLINE_AFTER_FAILS) {
        s_fail_count++;
    }
    if (s_fail_count >= AXION_OFFLINE_AFTER_FAILS && !s_offline) {
        s_offline = true;
        ESP_LOGW(TAG, "OFFLINE state (%d consecutive failures)", s_fail_count);
    }
}

static void mark_ok(void)
{
    s_fail_count = 0;
    if (s_offline) {
        s_offline = false;
        ESP_LOGI(TAG, "ONLINE state");
    }
}

static int build_body(char *buf, size_t cap, const rssi_snapshot_t *snap,
                      int64_t now_ms)
{
    int pos = snprintf(buf, cap,
                       "{\"atom_id\":\"%s\",\"zone\":\"%s\",\"seq\":%" PRIu32
                       ",\"uptime_ms\":%" PRId64 ",\"beacons\":[",
                       CONFIG_AXION_ATOM_ID, CONFIG_AXION_ZONE, s_seq, now_ms);
    for (size_t i = 0; i < snap->count && pos < (int)cap; i++) {
        char mac_s[18];
        fmt_mac(snap->entries[i].mac, mac_s);
        pos += snprintf(&buf[pos], cap - (size_t)pos,
                        "%s{\"mac\":\"%s\",\"rssi\":%.1f,\"n\":%u}",
                        i ? "," : "", mac_s,
                        (double)snap->entries[i].rssi, snap->entries[i].n);
    }
    if (pos < (int)cap) {
        pos += snprintf(&buf[pos], cap - (size_t)pos, "]}");
    }
    return pos;
}

static int json_int_field(const char *json, const char *key)
{
    char pat[32];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = strstr(json, pat);
    if (p == NULL) {
        return -1;
    }
    p = strchr(p + strlen(pat), ':');
    if (p == NULL) {
        return -1;
    }
    return atoi(p + 1);
}

static esp_http_client_handle_t client_get(void)
{
    if (s_client != NULL) {
        return s_client;
    }
    esp_http_client_config_t cfg = {
        .url = s_url_readings,
        .method = HTTP_METHOD_POST,
        .timeout_ms = AXION_HTTP_TIMEOUT_MS,
        .keep_alive_enable = true,
    };
    s_client = esp_http_client_init(&cfg);
    if (s_client != NULL) {
        esp_http_client_set_header(s_client, "Content-Type", "application/json");
    }
    return s_client;
}

static void client_drop(void)
{
    if (s_client != NULL) {
        esp_http_client_cleanup(s_client);
        s_client = NULL;
    }
}

static bool post_snapshot(const char *body, int body_len,
                          char *resp, size_t resp_cap)
{
    esp_http_client_handle_t c = client_get();
    if (c == NULL) {
        return false;
    }
    if (esp_http_client_open(c, body_len) != ESP_OK) {
        client_drop();
        return false;
    }
    if (esp_http_client_write(c, body, body_len) != body_len) {
        esp_http_client_close(c);
        return false;
    }
    if (esp_http_client_fetch_headers(c) < 0) {
        esp_http_client_close(c);
        return false;
    }
    int status = esp_http_client_get_status_code(c);
    int n = esp_http_client_read_response(c, resp, (int)resp_cap - 1);
    if (n < 0) {
        n = 0;
    }
    resp[n] = '\0';
    if (status != 200) {
        ESP_LOGW(TAG, "POST status=%d resp=%s", status, resp);
        esp_http_client_close(c);
        return false;
    }
    return true;
}

static void fetch_whitelist(void)
{
    esp_http_client_config_t cfg = {
        .url = s_url_beacons,
        .method = HTTP_METHOD_GET,
        .timeout_ms = AXION_HTTP_TIMEOUT_MS,
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (c == NULL) {
        return;
    }
    static char resp[512];
    int len = 0;
    if (esp_http_client_open(c, 0) == ESP_OK &&
        esp_http_client_fetch_headers(c) >= 0 &&
        esp_http_client_get_status_code(c) == 200) {
        len = esp_http_client_read_response(c, resp, sizeof(resp) - 1);
    }
    esp_http_client_cleanup(c);
    if (len <= 0) {
        ESP_LOGW(TAG, "failed to fetch whitelist from API");
        return;
    }
    resp[len] = '\0';

    int ver = json_int_field(resp, "ver");

    static char        macbuf[AXION_WHITELIST_API_MAX][18];
    const char        *macs[AXION_WHITELIST_API_MAX];
    size_t             n = 0;
    const char *p = strstr(resp, "\"macs\"");
    if (p) {
        p = strchr(p, '[');
    }
    while (p && n < AXION_WHITELIST_API_MAX) {
        const char *q = strchr(p, '"');
        if (q == NULL) {
            break;
        }
        const char *end = strchr(q + 1, '"');
        if (end == NULL) {
            break;
        }
        size_t l = (size_t)(end - (q + 1));
        if (l > 0 && l < sizeof(macbuf[0])) {
            memcpy(macbuf[n], q + 1, l);
            macbuf[n][l] = '\0';
            macs[n] = macbuf[n];
            n++;
        }
        p = end + 1;
        if (strchr(p, '"') == NULL || (strchr(p, ']') && strchr(p, ']') < strchr(p, '"'))) {
            break;
        }
    }

    ble_scan_set_whitelist(macs, n);
    if (ver >= 0) {
        s_whitelist_ver = ver;
    }
    ESP_LOGI(TAG, "whitelist from API applied: ver=%d, %u MAC(s)", ver, (unsigned)n);
}

static int local_count_hyst(const rssi_snapshot_t *snap)
{
    bool seen[RSSI_SNAPSHOT_MAX] = { false };
    for (size_t i = 0; i < snap->count; i++) {
        int idx = -1;
        for (size_t k = 0; k < s_lc_n; k++) {
            if (memcmp(s_lc_mac[k], snap->entries[i].mac, 6) == 0) {
                idx = (int)k;
                break;
            }
        }
        if (idx < 0 && s_lc_n < RSSI_SNAPSHOT_MAX) {
            idx = (int)s_lc_n++;
            memcpy(s_lc_mac[idx], snap->entries[i].mac, 6);
            s_lc_on[idx] = false;
        }
        if (idx < 0) {
            continue;
        }
        float r = snap->entries[i].rssi;
        if (r >= AXION_LOCAL_COUNT_ENTER_DBM) {
            s_lc_on[idx] = true;
        } else if (r < AXION_LOCAL_COUNT_EXIT_DBM) {
            s_lc_on[idx] = false;
        }
        seen[idx] = true;
    }
    int count = 0;
    size_t w = 0;
    for (size_t k = 0; k < s_lc_n; k++) {
        if (!seen[k]) {
            continue;
        }
        memcpy(s_lc_mac[w], s_lc_mac[k], 6);
        s_lc_on[w] = s_lc_on[k];
        if (s_lc_on[w]) {
            count++;
        }
        w++;
    }
    s_lc_n = w;
    return count;
}

static void log_snapshot(const rssi_snapshot_t *snap)
{
    if (snap->count == 0) {
        ESP_LOGI(TAG_FILT, "empty snapshot");
        return;
    }
    char line[224];
    int  pos = 0;
    for (size_t i = 0; i < snap->count && pos < (int)sizeof(line) - 40; i++) {
        char mac_s[18];
        fmt_mac(snap->entries[i].mac, mac_s);
        pos += snprintf(&line[pos], sizeof(line) - (size_t)pos,
                        "%s%s=%.1f(n=%u)", i ? "  " : "", mac_s,
                        (double)snap->entries[i].rssi, snap->entries[i].n);
    }
    ESP_LOGI(TAG_FILT, "snapshot: %s", line);
}

static void net_task(void *arg)
{
    static char body[768];
    static char resp[256];
    TickType_t last = xTaskGetTickCount();

    for (;;) {
        vTaskDelayUntil(&last, pdMS_TO_TICKS(AXION_SNAPSHOT_PERIOD_MS));
        int64_t now_ms = esp_timer_get_time() / 1000;

        rssi_snapshot_t snap;
        rssi_filter_snapshot(&snap, now_ms);

        s_local_count = local_count_hyst(&snap);
        log_snapshot(&snap);
        AXION_DBG(TAG, "counters: local=%d zone=%d offline=%d",
                  s_local_count, s_zone_count, (int)s_offline);

        if (!s_wifi_enabled) {
            continue;
        }
        if (!s_connected) {
            try_reconnect(now_ms);
            mark_fail();
            continue;
        }

        if (s_whitelist_ver < 0 ||
            now_ms - s_last_wl_refresh_ms > AXION_WHITELIST_REFRESH_S * 1000) {
            fetch_whitelist();
            s_last_wl_refresh_ms = now_ms;
        }

        int len = build_body(body, sizeof(body), &snap, now_ms);
        s_seq++;
        if (post_snapshot(body, len, resp, sizeof(resp))) {
            int zc = json_int_field(resp, "zone_count");
            if (zc >= 0) {
                s_zone_count = zc;
            }
            int wv = json_int_field(resp, "whitelist_ver");
            if (wv >= 0 && wv != s_whitelist_ver) {
                ESP_LOGI(TAG, "whitelist_ver changed (%d -> %d), refetching",
                         s_whitelist_ver, wv);
                fetch_whitelist();
                s_last_wl_refresh_ms = now_ms;
            }
            mark_ok();
        } else {
            mark_fail();
        }
    }
}

net_state_t net_state(void)
{
    if (!s_wifi_enabled) {
        return NET_DISABLED;
    }
    if (!s_ever_connected) {
        return NET_CONNECTING;
    }
    return s_offline ? NET_OFFLINE : NET_ONLINE;
}

int net_zone_count(void)
{
    return s_zone_count;
}

int net_local_count(void)
{
    return s_local_count;
}

esp_err_t net_init(void)
{
    snprintf(s_url_readings, sizeof(s_url_readings), "%s/api/readings", CONFIG_AXION_API_URL);
    snprintf(s_url_beacons, sizeof(s_url_beacons), "%s/api/beacons/active", CONFIG_AXION_API_URL);
    s_wifi_enabled = strlen(CONFIG_AXION_WIFI_SSID) > 0;

    if (!s_wifi_enabled) {
        ESP_LOGW(TAG, "empty SSID: Wi-Fi DISABLED (configure in "
                      "idf.py menuconfig > AXION). Running without the API.");
    } else {
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        esp_netif_create_default_wifi_sta();

        wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&wcfg));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(
            WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, NULL));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(
            IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL, NULL));

        wifi_config_t sta = { 0 };
        strncpy((char *)sta.sta.ssid, CONFIG_AXION_WIFI_SSID,
                sizeof(sta.sta.ssid) - 1);
        strncpy((char *)sta.sta.password, CONFIG_AXION_WIFI_PASSWORD,
                sizeof(sta.sta.password) - 1);
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta));
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MIN_MODEM));
        ESP_LOGI(TAG, "Wi-Fi STA started, SSID=%s API=%s",
                 CONFIG_AXION_WIFI_SSID, s_url_readings);
    }

    if (xTaskCreatePinnedToCore(net_task, "t_net", 6144, NULL, 5, NULL, 1)
        != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
