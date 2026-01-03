#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "device_config.h"
#include "display_driver.h"
#include "display_ui.h"
#include "motion_sensor.h"
#include "http_server.h"
#include "esp_now_device.h"

static const char *TAG = "main";

// Forward declarations
static void init_wifi(void);
static void on_motion_event(const motion_event_t *event);
static void main_event_loop(void *pvParameters);

/**
 * Initialize WiFi in STA mode for ESP-NOW communication
 * If device is not configured, start AP mode instead
 */
static void init_wifi(void)
{
    const device_config_t *config = device_config_get();
    
    ESP_LOGI(TAG, "Initializing WiFi...");
    
    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    if (!device_config_is_configured()) {
        // Start AP mode for setup
        ESP_LOGW(TAG, "Device not configured - starting AP mode");
        
        wifi_config_t ap_config = {
            .ap = {
                .authmode = WIFI_AUTH_OPEN,
                .max_connection = 4,
            }
        };
        
        // Create AP SSID from device ID
        uint8_t mac[6];
        esp_wifi_get_mac(WIFI_IF_AP, mac);
        snprintf((char *)ap_config.ap.ssid, sizeof(ap_config.ap.ssid),
                 "ESP32-MESH-%02X%02X%02X", mac[3], mac[4], mac[5]);
        ap_config.ap.ssid_len = strlen((char *)ap_config.ap.ssid);
        
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
        ESP_ERROR_CHECK(esp_wifi_start());
        
        ESP_LOGI(TAG, "AP Mode started: %s", ap_config.ap.ssid);
    } else {
        // Start STA mode for normal operation
        ESP_LOGI(TAG, "Device configured - starting STA mode for ESP-NOW");
        
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
    }
}

/**
 * Callback when motion is detected
 */
static void on_motion_event(const motion_event_t *event)
{
    ESP_LOGI(TAG, "Motion event callback - motion=%d", event->motion_detected);
}

/**
 * Main event loop - updates display and sends heartbeats
 */
static void main_event_loop(void *pvParameters)
{
    const device_config_t *config = device_config_get();
    uint32_t heartbeat_count = 0;
    uint32_t last_display_update = xTaskGetTickCount();
    uint32_t last_heartbeat = xTaskGetTickCount();
    
    ESP_LOGI(TAG, "Main event loop started");
    
    while (1) {
        uint32_t now = xTaskGetTickCount();
        
        // Update display every 500ms with motion status
        if ((now - last_display_update) / portTICK_PERIOD_MS > 500) {
            bool motion = motion_sensor_is_motion_detected();
            uint32_t time_since = motion_sensor_time_since_motion();
            
            display_ui_show_motion_status(motion, time_since);
            
            last_display_update = now;
        }
        
        // Send heartbeat every 30 seconds
        if ((now - last_heartbeat) / portTICK_PERIOD_MS > 30000) {
            if (device_config_is_configured()) {
                // In production, would send to home base
                // esp_now_device_send_heartbeat(home_base_mac, now / 1000);
                ESP_LOGD(TAG, "Heartbeat #%ld", ++heartbeat_count);
            }
            last_heartbeat = now;
        }
        
        // Process any queued ESP-NOW messages
        esp_now_device_process_messages(NULL);
        
        // Sleep a bit to avoid busy-waiting
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== ESP32-C6 Motion Sensor Firmware Start ===");
    
    // 1. Initialize configuration system
    ESP_ERROR_CHECK(device_config_init());
    ESP_ERROR_CHECK(device_config_load());
    
    const device_config_t *config = device_config_get();
    ESP_LOGI(TAG, "Device ID: %s", config->device_id);
    ESP_LOGI(TAG, "Motion GPIO: %d", config->motion_gpio);
    ESP_LOGI(TAG, "Display Enabled: %d", config->display_enabled);
    
    // 2. Initialize display
    if (config->display_enabled) {
        ESP_LOGI(TAG, "Initializing display...");
        if (display_init() != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize display");
        } else {
            display_ui_init();
            display_ui_show_splash("ESP32-C6 Motion Sensor");
        }
    }
    
    // 3. Initialize motion sensor
    ESP_LOGI(TAG, "Initializing motion sensor...");
    ESP_ERROR_CHECK(motion_sensor_init(on_motion_event));
    motion_sensor_start_task();
    
    // 4. Initialize networking
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    init_wifi();
    
    // 5. Start HTTP server
    ESP_LOGI(TAG, "Starting HTTP configuration server...");
    ESP_ERROR_CHECK(http_server_start());
    
    // 6. Initialize ESP-NOW (for configured devices)
    if (device_config_is_configured()) {
        ESP_LOGI(TAG, "Initializing ESP-NOW for mesh communication...");
        ESP_ERROR_CHECK(esp_now_device_init());
        
        // In production, would add home base as peer
        // uint8_t home_base_mac[6] = {0x...};
        // esp_now_device_add_peer(home_base_mac);
        
        display_ui_show_network_info(config->network_id, config->device_id);
    } else {
        display_ui_show_connecting("Waiting for configuration...");
    }
    
    // 7. Start main event loop
    ESP_LOGI(TAG, "Starting main event loop...");
    xTaskCreate(main_event_loop, "event_loop", 4096, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "=== Device Ready ===");
    ESP_LOGI(TAG, "Visit http://192.168.4.1 to configure device");
}
