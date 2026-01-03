#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/**
 * Device configuration structure for ESP32-C6 motion sensor with TFT display
 * Persisted in NVS as JSON under "device" namespace, key "config"
 */
typedef struct {
    // Device identification
    char device_id[32];
    int network_id;
    uint8_t type;
    uint8_t home_base_mac[6];
    
    // Motion sensor configuration
    uint8_t motion_gpio;
    uint8_t motion_sensitivity;      // 1-10 scale
    uint32_t motion_cooldown_ms;     // 5000-300000ms
    
    // Display configuration (ST7789 via SPI)
    bool display_enabled;
    uint16_t display_width;
    uint16_t display_height;
    
    // SPI pins for display
    uint8_t display_sclk_gpio;
    uint8_t display_mosi_gpio;
    uint8_t display_miso_gpio;
    uint8_t display_cs_gpio;
    uint8_t display_dc_gpio;         // Data/Command control
    uint8_t display_reset_gpio;
    uint8_t display_backlight_gpio;
    uint8_t display_brightness;      // 0-100
    
    // Display colors (RGB565)
    uint16_t color_motion;           // Motion detected color
    uint16_t color_clear;            // No motion color
    uint16_t color_cooldown;         // Cooldown period color
    uint16_t color_text;             // Text color
    uint16_t color_background;       // Background color
    
    char board_variant[32];
} device_config_t;

/**
 * Initialize NVS flash for configuration storage
 */
esp_err_t device_config_init(void);

/**
 * Load configuration from NVS (or use defaults if not found)
 */
esp_err_t device_config_load(void);

/**
 * Save configuration to NVS
 */
esp_err_t device_config_save(const device_config_t *config);

/**
 * Get current configuration (read-only)
 */
const device_config_t* device_config_get(void);

/**
 * Check if device has been registered with a network
 */
bool device_config_is_configured(void);

/**
 * Update motion GPIO and save
 */
void device_config_update_motion_gpio(uint8_t gpio);

/**
 * Update display colors and save
 */
void device_config_update_display_colors(uint16_t motion, uint16_t clear, uint16_t cooldown, uint16_t text);

/**
 * Update display brightness (0-100) and save
 */
void device_config_update_display_brightness(uint8_t brightness);

#endif // DEVICE_CONFIG_H
