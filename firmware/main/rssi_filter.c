#include "rssi_filter.h"

#include <math.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "axion_config.h"

#define MAX_BEACONS  8
#define MAX_SAMPLES  32

typedef struct {
    bool    in_use;
    uint8_t mac[6];
    int8_t  rssi[MAX_SAMPLES];
    int64_t ts[MAX_SAMPLES];
    uint8_t head;
    uint8_t count;
    float   ema;
    bool    ema_init;
    int64_t last_seen;
} beacon_buf_t;

static beacon_buf_t s_bufs[MAX_BEACONS];
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

void rssi_filter_init(void)
{
    memset(s_bufs, 0, sizeof(s_bufs));
}

void rssi_filter_add(const uint8_t mac[6], int8_t rssi, int64_t now_ms)
{
    taskENTER_CRITICAL(&s_mux);

    beacon_buf_t *b = NULL;
    beacon_buf_t *free_slot = NULL;
    beacon_buf_t *oldest = &s_bufs[0];
    for (int i = 0; i < MAX_BEACONS; i++) {
        if (s_bufs[i].in_use) {
            if (memcmp(s_bufs[i].mac, mac, 6) == 0) {
                b = &s_bufs[i];
                break;
            }
            if (s_bufs[i].last_seen < oldest->last_seen) {
                oldest = &s_bufs[i];
            }
        } else if (free_slot == NULL) {
            free_slot = &s_bufs[i];
        }
    }
    if (b == NULL) {
        b = free_slot ? free_slot : oldest;
        memset(b, 0, sizeof(*b));
        b->in_use = true;
        memcpy(b->mac, mac, 6);
    }

    b->rssi[b->head] = rssi;
    b->ts[b->head]   = now_ms;
    b->head = (b->head + 1) % MAX_SAMPLES;
    if (b->count < MAX_SAMPLES) {
        b->count++;
    }
    if (!b->ema_init) {
        b->ema = (float)rssi;
        b->ema_init = true;
    } else {
        b->ema = AXION_RSSI_EMA_ALPHA * (float)rssi
                 + (1.0f - AXION_RSSI_EMA_ALPHA) * b->ema;
    }
    b->last_seen = now_ms;

    taskEXIT_CRITICAL(&s_mux);
}

void rssi_filter_snapshot(rssi_snapshot_t *out, int64_t now_ms)
{
    out->count = 0;

    for (int i = 0; i < MAX_BEACONS; i++) {
        int8_t  vals[MAX_SAMPLES];
        int     n = 0;
        uint8_t mac[6];
        float   ema;

        taskENTER_CRITICAL(&s_mux);
        beacon_buf_t *b = &s_bufs[i];
        if (!b->in_use) {
            taskEXIT_CRITICAL(&s_mux);
            continue;
        }
        if (now_ms - b->last_seen > AXION_RSSI_WINDOW_MS) {
            b->in_use = false;
            taskEXIT_CRITICAL(&s_mux);
            continue;
        }
        for (int k = 0; k < b->count; k++) {
            if (now_ms - b->ts[k] <= AXION_RSSI_WINDOW_MS) {
                vals[n++] = b->rssi[k];
            }
        }
        memcpy(mac, b->mac, 6);
        ema = b->ema;
        taskEXIT_CRITICAL(&s_mux);

        if (n == 0) {
            continue;
        }

        float sum = 0.0f;
        for (int k = 0; k < n; k++) {
            sum += (float)vals[k];
        }
        float mean = sum / (float)n;
        float var = 0.0f;
        for (int k = 0; k < n; k++) {
            float d = (float)vals[k] - mean;
            var += d * d;
        }
        float sd = sqrtf(var / (float)n);

        int8_t kept[MAX_SAMPLES];
        int    kn = 0;
        for (int k = 0; k < n; k++) {
            if (sd < 0.5f ||
                fabsf((float)vals[k] - mean) <= AXION_RSSI_OUTLIER_SIGMA * sd) {
                kept[kn++] = vals[k];
            }
        }
        if (kn == 0) {
            memcpy(kept, vals, (size_t)n);
            kn = n;
        }

        for (int a = 1; a < kn; a++) {
            int8_t v = kept[a];
            int    j = a - 1;
            while (j >= 0 && kept[j] > v) {
                kept[j + 1] = kept[j];
                j--;
            }
            kept[j + 1] = v;
        }
        float median = (kn % 2) ? (float)kept[kn / 2]
                                : ((float)kept[kn / 2 - 1] + (float)kept[kn / 2]) / 2.0f;

#if AXION_RSSI_FILTER == AXION_RSSI_FILTER_EMA
        float filtered = ema;
        (void)median;
#else
        float filtered = median;
        (void)ema;
#endif

        if (out->count < RSSI_SNAPSHOT_MAX) {
            rssi_entry_t *e = &out->entries[out->count++];
            memcpy(e->mac, mac, 6);
            e->rssi = filtered;
            e->n = (uint16_t)kn;
        }
    }
}
