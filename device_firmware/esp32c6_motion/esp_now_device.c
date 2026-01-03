#include <string.h>
#include "esp_log.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "device_config.h"
#include "esp_now_device.h"
#include "../home_base_firmware/main/include/protocol.h"

static const char *TAG = "esp_now_device";

// Message queue for received messages
static QueueHandle_t esp_now_queue = NULL;
#define ESP_NOW_QUEUE_SIZE 20

// Callback for received messages
static void on_data_recv(const uint8_t *mac_addr, const uint8_t *data, int len)
{
    if (len != sizeof(mesh_message_t)) {
        ESP_LOGW(TAG, "Invalid message size: %d != %d", len, (int)sizeof(mesh_message_t));
        return;
    }

    mesh_message_t msg;
    memcpy(&msg, data, sizeof(mesh_message_t));
    
    // Queue for processing
    if (xQueueSend(esp_now_queue, &msg, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "ESP-NOW queue full, dropping message");
    }
}

// Callback for sent messages
static void on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGD(TAG, "ESP-NOW message sent successfully");
    } else {
        ESP_LOGW(TAG, "ESP-NOW send failed");
    }
}

esp_err_t esp_now_device_init(void)
{
    ESP_LOGI(TAG, "Initializing ESP-NOW device mode...");
    
    // Create message queue
    esp_now_queue = xQueueCreate(ESP_NOW_QUEUE_SIZE, sizeof(mesh_message_t));
    if (!esp_now_queue) {
        ESP_LOGE(TAG, "Failed to create ESP-NOW queue");
        return ESP_ERR_NO_MEM;
    }

    // Initialize WiFi (required for ESP-NOW)
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
    
    // Initialize ESP-NOW
    if (esp_now_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ESP-NOW");
        return ESP_FAIL;
    }

    // Register callbacks
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_data_recv));
    ESP_ERROR_CHECK(esp_now_register_send_cb(on_data_sent));
    
    ESP_LOGI(TAG, "ESP-NOW device initialized");
    return ESP_OK;
}

esp_err_t esp_now_device_add_peer(const uint8_t *peer_mac)
{
    esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
    if (peer == NULL) {
        return ESP_ERR_NO_MEM;
    }

    memset(peer, 0, sizeof(esp_now_peer_info_t));
    memcpy(peer->peer_addr, peer_mac, 6);
    peer->channel = 0;
    peer->ifidx = WIFI_IF_STA;
    peer->encrypt = false;

    esp_err_t ret = esp_now_add_peer(peer);
    free(peer);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Peer added: %02X:%02X:%02X:%02X:%02X:%02X",
                 peer_mac[0], peer_mac[1], peer_mac[2],
                 peer_mac[3], peer_mac[4], peer_mac[5]);
    } else {
        ESP_LOGE(TAG, "Failed to add peer: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

esp_err_t esp_now_device_send_motion_event(const uint8_t *peer_mac, 
                                            uint32_t timestamp, 
                                            bool motion_detected,
                                            const uint8_t *signature)
{
    mesh_message_t msg;
    memset(&msg, 0, sizeof(mesh_message_t));
    
    const device_config_t *config = device_config_get();
    
    // Fill message
    msg.type = MSG_TYPE_MOTION;
    strncpy(msg.device_id, config->device_id, sizeof(msg.device_id) - 1);
    msg.timestamp = timestamp;
    
    // Create motion event payload JSON
    snprintf(msg.payload, sizeof(msg.payload), 
             "{\"motion\":%s,\"sensitivity\":%d,\"cooldown\":%d}",
             motion_detected ? "true" : "false",
             config->motion_sensitivity,
             config->motion_cooldown_ms);
    
    // Copy signature
    if (signature) {
        memcpy(msg.signature, signature, sizeof(msg.signature));
    }
    
    // Send via ESP-NOW
    esp_err_t ret = esp_now_send(peer_mac, (uint8_t *)&msg, sizeof(msg));
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Motion event sent: motion=%s", motion_detected ? "true" : "false");
    } else {
        ESP_LOGE(TAG, "Failed to send motion event: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

esp_err_t esp_now_device_send_heartbeat(const uint8_t *peer_mac, uint32_t timestamp)
{
    mesh_message_t msg;
    memset(&msg, 0, sizeof(mesh_message_t));
    
    const device_config_t *config = device_config_get();
    
    // Fill message
    msg.type = MSG_TYPE_HEARTBEAT;
    strncpy(msg.device_id, config->device_id, sizeof(msg.device_id) - 1);
    msg.timestamp = timestamp;
    
    // Create heartbeat payload
    uint32_t heap = esp_get_free_heap_size();
    snprintf(msg.payload, sizeof(msg.payload), 
             "{\"heap\":%lu,\"uptime\":%lu}",
             heap, timestamp);
    
    // Send via ESP-NOW
    esp_err_t ret = esp_now_send(peer_mac, (uint8_t *)&msg, sizeof(msg));
    
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "Heartbeat sent (heap=%lu)", heap);
    } else {
        ESP_LOGE(TAG, "Failed to send heartbeat: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

void esp_now_device_process_messages(esp_now_message_handler_t handler)
{
    mesh_message_t msg;
    
    // Process all queued messages
    while (xQueueReceive(esp_now_queue, &msg, 0) == pdTRUE) {
        ESP_LOGD(TAG, "Processing message type=0x%02x from %s", msg.type, msg.device_id);
        
        // Call user handler
        if (handler) {
            handler(&msg);
        }
        
        // Handle known types
        switch (msg.type) {
            case MSG_TYPE_COMMAND:
                ESP_LOGI(TAG, "Command received: %s", msg.payload);
                break;
            
            case MSG_TYPE_LOG:
                ESP_LOGD(TAG, "Log message: %s", msg.payload);
                break;
            
            default:
                ESP_LOGW(TAG, "Unknown message type: 0x%02x", msg.type);
        }
    }
}

void esp_now_device_deinit(void)
{
    esp_now_deinit();
    
    if (esp_now_queue) {
        vQueueDelete(esp_now_queue);
        esp_now_queue = NULL;
    }
}
