#include <string.h>
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "protocol.h"

static const char *TAG = "esp_now";

// Message queue for handling received messages asynchronously
static QueueHandle_t s_mesh_queue = NULL;
#define MESH_QUEUE_SIZE 20

// Forward declaration
void send_log_to_unraid(mesh_message_t *msg);

// Callback when data is received
static void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) {
    if (len != sizeof(mesh_message_t)) {
        ESP_LOGE(TAG, "Invalid message size: %d != %d", len, (int)sizeof(mesh_message_t));
        return;
    }

    // Queue the message for processing
    mesh_message_t msg;
    memcpy(&msg, incomingData, sizeof(mesh_message_t));
    
    if (xQueueSend(s_mesh_queue, &msg, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Message queue full, dropping message from %s", msg.device_id);
    }
}

// Task to process received messages
static void mesh_processing_task(void *pvParameters) {
    mesh_message_t msg;
    
    while (1) {
        if (xQueueReceive(s_mesh_queue, &msg, pdMS_TO_TICKS(1000))) {
            ESP_LOGI(TAG, "Processing message type=0x%02x from %s", msg.type, msg.device_id);
            
            // Route message based on type
            switch (msg.type) {
                case MSG_TYPE_HEARTBEAT:
                    ESP_LOGD(TAG, "Heartbeat from %s", msg.device_id);
                    // In production: update device status, RSSI, etc.
                    break;
                    
                case MSG_TYPE_MOTION:
                    ESP_LOGI(TAG, "Motion event from %s", msg.device_id);
                    send_log_to_unraid(&msg);
                    break;
                    
                case MSG_TYPE_LOG:
                    ESP_LOGD(TAG, "Log from %s: %s", msg.device_id, msg.payload);
                    send_log_to_unraid(&msg);
                    break;
                    
                case MSG_TYPE_COMMAND:
                    ESP_LOGI(TAG, "Command received: %s", msg.payload);
                    // In production: validate signature and execute command
                    break;
                    
                default:
                    ESP_LOGW(TAG, "Unknown message type: 0x%02x", msg.type);
            }
        }
    }
}

void init_esp_now(void) {
    // Create message queue
    s_mesh_queue = xQueueCreate(MESH_QUEUE_SIZE, sizeof(mesh_message_t));
    if (!s_mesh_queue) {
        ESP_LOGE(TAG, "Failed to create message queue");
        return;
    }

    // Initialize Wi-Fi in Station mode (required for ESP-NOW)
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    // Initialize ESP-NOW
    if (esp_now_init() != ESP_OK) {
        ESP_LOGE(TAG, "Error initializing ESP-NOW");
        return;
    }
    
    esp_now_register_recv_cb(OnDataRecv);
    ESP_LOGI(TAG, "ESP-NOW Initialized in STA mode");

    // Create message processing task
    xTaskCreate(mesh_processing_task, "mesh_proc", 4096, NULL, 5, NULL);
}
