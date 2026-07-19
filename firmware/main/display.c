#include "display.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "led_strip.h"

#include "axion_config.h"
#include "font5x5.h"

static const char *TAG = "DISP";

static led_strip_handle_t s_strip;

static inline uint8_t scale(uint8_t v)
{
    return (uint8_t)(((uint16_t)v * AXION_LED_BRIGHTNESS_MAX) / 255);
}

esp_err_t display_init(void)
{
    led_strip_config_t strip_cfg = {
        .strip_gpio_num = AXION_LED_GPIO,
        .max_leds = AXION_LED_COUNT,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = { .invert_out = false },
    };
    led_strip_rmt_config_t rmt_cfg = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .flags = { .with_dma = false },
    };

    esp_err_t err = led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &s_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "led_strip_new_rmt_device: %s", esp_err_to_name(err));
        return err;
    }
    return led_strip_clear(s_strip);
}

void display_sweep(uint32_t step_ms)
{
    ESP_LOGI(TAG, "mapping sweep: index 0..%d (top-left, row by row)",
             AXION_LED_COUNT - 1);
    for (int i = 0; i < AXION_LED_COUNT; i++) {
        led_strip_clear(s_strip);
        led_strip_set_pixel(s_strip, i, scale(255), scale(255), scale(255));
        led_strip_refresh(s_strip);
        vTaskDelay(pdMS_TO_TICKS(step_ms));
    }
    led_strip_clear(s_strip);
}

bool display_draw_glyph(char c, uint8_t r, uint8_t g, uint8_t b)
{
    const uint8_t *rows = font5x5_get(c);
    if (rows == NULL) {
        ESP_LOGW(TAG, "unknown glyph: '%c'", c);
        return false;
    }
    int lit = 0;
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 5; col++) {
            bool on = rows[row] & (1 << (4 - col));
            int idx = row * 5 + col;
            led_strip_set_pixel(s_strip, idx,
                                on ? scale(r) : 0,
                                on ? scale(g) : 0,
                                on ? scale(b) : 0);
            if (on) {
                lit++;
            }
        }
    }
    esp_err_t err = led_strip_refresh(s_strip);
    AXION_DBG(TAG, "draw '%c' lit=%d rgb=%u,%u,%u refresh=%s",
              c, lit, r, g, b, esp_err_to_name(err));
    return err == ESP_OK;
}

void display_fill(uint8_t r, uint8_t g, uint8_t b)
{
    for (int i = 0; i < AXION_LED_COUNT; i++) {
        led_strip_set_pixel(s_strip, i, scale(r), scale(g), scale(b));
    }
    led_strip_refresh(s_strip);
}

void display_clear(void)
{
    led_strip_clear(s_strip);
}

void display_center(uint8_t r, uint8_t g, uint8_t b)
{
    led_strip_clear(s_strip);
    led_strip_set_pixel(s_strip, 12, scale(r), scale(g), scale(b));
    led_strip_refresh(s_strip);
}

void display_zone_color(char zone, uint8_t *r, uint8_t *g, uint8_t *b)
{
    switch (zone) {
    case 'A': *r = 0;   *g = 200; *b = 255; break;
    case 'B': *r = 0;   *g = 220; *b = 80;  break;
    case 'C': *r = 255; *g = 120; *b = 0;   break;
    default:  *r = 255; *g = 255; *b = 255; break;
    }
}
