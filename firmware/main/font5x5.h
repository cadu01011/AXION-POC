/**
 * @file font5x5.h
 * @brief 5x5 bitmap font for the LED matrix (letters A-C and digits 0-9).
 *
 * Each glyph is five rows; rows[0] is the top row, bit 4 is the leftmost
 * column and bit 0 the rightmost.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>

/** @brief One glyph: its character and five row bitmaps. */
typedef struct {
    char    c;
    uint8_t rows[5];
} font5x5_glyph_t;

static const font5x5_glyph_t FONT5X5[] = {
    { 'A', { 0x0E, 0x11, 0x1F, 0x11, 0x11 } },
    { 'B', { 0x1E, 0x11, 0x1E, 0x11, 0x1E } },
    { 'C', { 0x0F, 0x10, 0x10, 0x10, 0x0F } },
    { '0', { 0x0E, 0x11, 0x11, 0x11, 0x0E } },
    { '1', { 0x04, 0x0C, 0x04, 0x04, 0x0E } },
    { '2', { 0x1E, 0x01, 0x0E, 0x10, 0x1F } },
    { '3', { 0x1E, 0x01, 0x06, 0x01, 0x1E } },
    { '4', { 0x12, 0x12, 0x1F, 0x02, 0x02 } },
    { '5', { 0x1F, 0x10, 0x1E, 0x01, 0x1E } },
    { '6', { 0x0E, 0x10, 0x1E, 0x11, 0x0E } },
    { '7', { 0x1F, 0x01, 0x02, 0x04, 0x04 } },
    { '8', { 0x0E, 0x11, 0x0E, 0x11, 0x0E } },
    { '9', { 0x0E, 0x11, 0x0F, 0x01, 0x0E } },
};

/**
 * @brief Look up the row bitmaps for a glyph.
 * @param c  Character to look up ('A'-'C' or '0'-'9').
 * @return   Pointer to the five row bytes, or NULL if the glyph is absent.
 */
static inline const uint8_t *font5x5_get(char c)
{
    for (size_t i = 0; i < sizeof(FONT5X5) / sizeof(FONT5X5[0]); i++) {
        if (FONT5X5[i].c == c) {
            return FONT5X5[i].rows;
        }
    }
    return NULL;
}
