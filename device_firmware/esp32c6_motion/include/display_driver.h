#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include <stdint.h>
#include "esp_err.h"

/**
 * Initialize ST7789 display (240x320)
 * Uses SPI3 with pins configured from device_config
 */
esp_err_t display_init(void);

/**
 * Fill entire display with a color (RGB565)
 */
void display_fill_color(uint16_t color);

/**
 * Set drawing window (clip region)
 * @param x Starting X coordinate
 * @param y Starting Y coordinate
 * @param w Width in pixels
 * @param h Height in pixels
 */
void display_set_window(uint16_t x, uint16_t y, uint16_t w, uint16_t h);

/**
 * Draw a single pixel
 * @param x X coordinate
 * @param y Y coordinate
 * @param color RGB565 color
 */
void display_draw_pixel(uint16_t x, uint16_t y, uint16_t color);

/**
 * Set display brightness (0-100)
 * NOTE: Currently just on/off via GPIO. PWM control can be added later.
 */
void display_set_brightness(uint8_t brightness);

/**
 * Deinit and cleanup SPI
 */
void display_deinit(void);

#endif // DISPLAY_DRIVER_H
