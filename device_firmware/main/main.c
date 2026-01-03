#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "device_firmware.h"

static const char *TAG = "app_main";

void app_main(void)
{
    ESP_LOGI(TAG, "=== ESP32-S3 Device Firmware ===");

    // Initialize device firmware (WiFi, ESP-NOW, config, sensors)
    device_firmware_init();

    const device_config_t *config = device_get_config();
    if (device_is_configured()) {
        ESP_LOGI(TAG, "Device configured: %s (network_id=%d)",
                 config->device_id, config->network_id);
        
        // Start main application loop
        while (1) {
            // Send heartbeat every 30 seconds
            device_send_heartbeat();
            vTaskDelay(pdMS_TO_TICKS(30000));
        }
    } else {
        ESP_LOGW(TAG, "Device not configured. Connect to AP 'ESP32-MESH-*' to configure.");
        
        // Device stays in AP mode for configuration
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(10000));
        }
    }
}
