#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    char device_id[32];          // Hardware ID/MAC
    uint32_t network_id;         // Home network ID from Unraid
    uint8_t type;                // 0x01=motion, 0x02=camera
    uint8_t pir_gpio;            // PIR sensor GPIO pin
    uint8_t pir_sensitivity;     // 1-10 scale
    uint32_t pir_cooldown_ms;    // Cooldown duration in ms
    uint8_t led_gpio;            // WS2812 LED GPIO pin
    uint8_t led_brightness;      // 0-100%
    bool camera_enabled;         // Camera enabled flag
    char board_variant[32];      // Board variant string
} device_config_t;

/**
 * Initialize NVS for configuration storage
 */
esp_err_t device_config_init(void);

/**
 * Load configuration from NVS (or use defaults if not present)
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
 * Check if device is configured (has network_id set)
 */
bool device_config_is_configured(void);

/**
 * Get configuration as JSON string (caller must free)
 */
char* device_config_to_json_string(void);

#endif // DEVICE_CONFIG_H
