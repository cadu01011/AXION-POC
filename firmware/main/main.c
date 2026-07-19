#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "axion_config.h"
#include "ble_scan.h"
#include "display.h"
#include "net.h"
#include "rssi_filter.h"

static const char *TAG = "MAIN";

static const char GLYPHS[] = "ABC0123456789";

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else {
        ESP_ERROR_CHECK(err);
    }

    ESP_LOGI(TAG, "AXION %s / zone %s starting (display + BLE scan)",
             CONFIG_AXION_ATOM_ID, CONFIG_AXION_ZONE);

    ESP_ERROR_CHECK(display_init());
    display_sweep(AXION_LED_SWEEP_STEP_MS);

    uint8_t r, g, b;
    const char zone = CONFIG_AXION_ZONE[0];
    display_zone_color(zone, &r, &g, &b);
    display_draw_glyph(zone, r, g, b);

    rssi_filter_init();
    ESP_ERROR_CHECK(ble_scan_init());
    ESP_ERROR_CHECK(net_init());

    gpio_config_t btn_cfg = {
        .pin_bit_mask = 1ULL << AXION_BTN_GPIO,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&btn_cfg));

    int     stable   = 1;
    int     last_raw = 1;
    int64_t last_change_us = 0;
    int64_t phase_start_us = esp_timer_get_time();
    bool    show_count = false;
    int     last_drawn_count = -1;
    bool    redraw = true;
    int     last_state = -1;
    bool    blink_on = false;
    int64_t blink_us = 0;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(10));
        int64_t now_us = esp_timer_get_time();

        int raw = gpio_get_level(AXION_BTN_GPIO);
        if (raw != last_raw) {
            last_raw = raw;
            last_change_us = now_us;
        }
        if (raw != stable &&
            (now_us - last_change_us) >= (int64_t)AXION_BTN_DEBOUNCE_MS * 1000) {
            stable = raw;
            if (stable == 0) {
                ESP_LOGI(TAG, "button: glyph test sequence");
                for (size_t i = 0; i < sizeof(GLYPHS) - 1; i++) {
                    display_draw_glyph(GLYPHS[i], r, g, b);
                    vTaskDelay(pdMS_TO_TICKS(300));
                }
                show_count = false;
                phase_start_us = esp_timer_get_time();
                redraw = true;
            }
        }

        int st = (int)net_state();
        if (st != last_state) {
            AXION_DBG(TAG, "display state -> %d (0=off 1=conn 2=online 3=offl)", st);
            last_state = st;
            redraw = true;
            show_count = false;
            phase_start_us = now_us;
        }

        if (st == NET_CONNECTING) {
            if (now_us - blink_us >= 500000) {
                blink_us = now_us;
                blink_on = !blink_on;
                if (blink_on) {
                    display_center(255, 200, 0);
                } else {
                    display_clear();
                }
            }
            continue;
        }

        if (st == NET_OFFLINE) {
            if (redraw) {
                display_fill(255, 0, 0);
                last_drawn_count = -1;
                redraw = false;
            }
            continue;
        }

        int64_t phase_ms = show_count ? AXION_DISPLAY_COUNT_MS
                                      : AXION_DISPLAY_LETTER_MS;
        if ((now_us - phase_start_us) / 1000 >= phase_ms) {
            show_count = !show_count;
            phase_start_us = now_us;
            redraw = true;
            AXION_DBG(TAG, "phase -> %s", show_count ? "COUNT" : "LETTER");
        }

        int count = (st == NET_ONLINE) ? net_zone_count() : net_local_count();
        if (count < 0) {
            count = 0;
        }
        if (show_count && count != last_drawn_count) {
            AXION_DBG(TAG, "count changed %d -> %d", last_drawn_count, count);
            redraw = true;
        }

        if (redraw) {
            if (show_count) {
                char digit = count > 9 ? '9' : (char)('0' + count);
                AXION_DBG(TAG, "render COUNT '%c'", digit);
                display_draw_glyph(digit, r, g, b);
                last_drawn_count = count;
            } else {
                AXION_DBG(TAG, "render LETTER '%c'", zone);
                display_draw_glyph(zone, r, g, b);
            }
            redraw = false;
        }
    }
}
