#include <string.h>
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "protocol.h"

static const char *TAG = "esp_now";

// Forward declaration
void send_log_to_unraid(mesh_message_t *msg);

// Callback when data is received
static void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) {
    if (len != sizeof(mesh_message_t)) {
        ESP_LOGE(TAG, "Invalid message size: %d != %d", len, sizeof(mesh_message_t));
        return;
    }

    mesh_message_t *msg = (mesh_message_t *)incomingData;
    ESP_LOGI(TAG, "Received msg type %d from %s", msg->type, msg->device_id);

    // If it's a log or motion event, forward to Unraid
    if (msg->type == MSG_TYPE_LOG || msg->type == MSG_TYPE_MOTION) {
        send_log_to_unraid(msg);
    }
}

void init_esp_now(void) {
    ESP_ERROR_CHECK(esp_netif_init());

    // Initialize Wi-Fi in Station mode (required for ESP-NOW)
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Initialize ESP-NOW
    if (esp_now_init() != ESP_OK) {
        ESP_LOGE(TAG, "Error initializing ESP-NOW");
        return;
    }
    esp_now_register_recv_cb(OnDataRecv);
    ESP_LOGI(TAG, "ESP-NOW Initialized in STA mode");
}
