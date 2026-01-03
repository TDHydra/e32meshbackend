#include <esp_http_client.h>
#include <esp_log.h>
#include <cJSON.h>
#include "protocol.h"
#include "sdkconfig.h"

static const char *TAG = "unraid_client";

// Use Kconfig value if available, else default (user should configure this in menuconfig)
#ifdef CONFIG_UNRAID_API_URL
    #define UNRAID_API_URL CONFIG_UNRAID_API_URL
#else
    #define UNRAID_API_URL "http://192.168.1.100:8000/logs/ingest"
#endif

void send_log_to_unraid(mesh_message_t *msg) {
    // 1. Serialize message to JSON matching LogIngestRequest
    cJSON *root = cJSON_CreateObject();
    cJSON *logs = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "logs", logs);

    cJSON *item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "device_id", msg->device_id);
    cJSON_AddNumberToObject(item, "timestamp", (double)msg->timestamp);
    cJSON_AddStringToObject(item, "level", "INFO"); // Parse from payload in real impl
    cJSON_AddStringToObject(item, "category", "system");
    cJSON_AddStringToObject(item, "message", msg->payload);

    // Hex encode the signature
    char signature_hex[129];
    for(int i=0; i<64; i++) {
        sprintf(&signature_hex[i*2], "%02x", msg->signature[i]);
    }
    cJSON_AddStringToObject(item, "signature", signature_hex);

    cJSON_AddItemToArray(logs, item);

    char *json_str = cJSON_PrintUnformatted(root);

    // 2. Send POST request
    esp_http_client_config_t config = {
        .url = UNRAID_API_URL,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_str, strlen(json_str));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Log sent, status = %d", esp_http_client_get_status_code(client));
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    cJSON_Delete(root);
    free(json_str);
}
