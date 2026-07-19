/**
 * @file display.h
 * @brief Driver for the ATOM Matrix 5x5 WS2812C LED display.
 *
 * Colors are passed at full scale (0-255); the global brightness cap
 * AXION_LED_BRIGHTNESS_MAX is applied internally on every write.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/**
 * @brief Initialize the LED matrix (RMT driver) and clear it.
 * @return ESP_OK on success, or the underlying led_strip error.
 */
esp_err_t display_init(void);

/**
 * @brief Light each LED in index order (0..24) to validate the physical
 *        mapping. Blocking; used once at boot.
 * @param step_ms  Delay between consecutive LEDs.
 */
void display_sweep(uint32_t step_ms);

/**
 * @brief Draw a 5x5 glyph ('A'-'C' or '0'-'9') in the given color.
 * @param c  Glyph character.
 * @param r  Red component (0-255, pre-brightness).
 * @param g  Green component (0-255, pre-brightness).
 * @param b  Blue component (0-255, pre-brightness).
 * @return   true if drawn, false if the glyph is not in the font.
 */
bool display_draw_glyph(char c, uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Fill the whole matrix with one color.
 * @param r  Red component (0-255, pre-brightness).
 * @param g  Green component (0-255, pre-brightness).
 * @param b  Blue component (0-255, pre-brightness).
 */
void display_fill(uint8_t r, uint8_t g, uint8_t b);

/** @brief Turn the whole matrix off. */
void display_clear(void);

/**
 * @brief Light only the center pixel (index 12); used for BOOT / connecting.
 * @param r  Red component (0-255, pre-brightness).
 * @param g  Green component (0-255, pre-brightness).
 * @param b  Blue component (0-255, pre-brightness).
 */
void display_center(uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Resolve a zone letter to its color (A cyan, B green, C orange).
 * @param zone  Zone character ('A', 'B' or 'C'); other values yield white.
 * @param[out] r  Red component.
 * @param[out] g  Green component.
 * @param[out] b  Blue component.
 */
void display_zone_color(char zone, uint8_t *r, uint8_t *g, uint8_t *b);
