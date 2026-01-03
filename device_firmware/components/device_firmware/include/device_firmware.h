#ifndef DEVICE_FIRMWARE_H
#define DEVICE_FIRMWARE_H

#include <stdint.h>
#include <stdbool.h>

// Device configuration stored in NVS
typedef struct {
    char device_id[32];             // Hardware ID (MAC address)
    uint32_t network_id;            // Home base network ID
    uint8_t device_type;            // 0x01=motion, 0x02=camera
    char private_key[256];          // Hex-encoded Ed25519 private key
    
    // PIR Sensor Config
    uint8_t pir_gpio;               // GPIO pin for PIR sensor
    uint8_t pir_sensitivity;        // 1-10 scale
    uint32_t pir_cooldown_ms;       // Cooldown duration
    bool pir_invert_logic;          // Some sensors are active-low
    
    // LED Config
    uint8_t led_gpio;               // WS2812 data pin
    uint8_t led_brightness;         // 0-100%
    char led_color_clear[7];        // Color when clear (hex RRGGBB)
    char led_color_cooldown[7];     // Color during cooldown
    char led_color_motion[7];       // Color when motion detected
    
    // Camera Config (if applicable)
    bool camera_enabled;
    uint8_t camera_sclk_gpio;       // SPI clock
    uint8_t camera_mosi_gpio;       // SPI MOSI
    uint8_t camera_miso_gpio;       // SPI MISO
    uint8_t camera_cs_gpio;         // SPI chip select
    uint8_t camera_pwdn_gpio;       // Power down
    
    // WiFi
    char ssid[32];
    char password[64];
} device_config_t;

// === Public API ===

/**
 * Initialize device firmware (config, WiFi, ESP-NOW, sensors)
 */
void device_firmware_init(void);

/**
 * Send heartbeat message to home base
 */
void device_send_heartbeat(void);

/**
 * Send motion event to home base
 */
void device_send_motion_event(void);

/**
 * Send log message to home base
 */
void device_send_log(const char *level, const char *category, const char *message);

/**
 * Get device configuration
 */
const device_config_t* device_get_config(void);

/**
 * Check if configured
 */
bool device_is_configured(void);

/**
 * Start WiFi AP mode for configuration portal
 */
void device_start_ap_mode(void);

/**
 * Start WiFi STA mode with saved credentials
 */
void device_start_sta_mode(void);

#endif // DEVICE_FIRMWARE_H
