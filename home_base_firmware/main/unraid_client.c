#include <esp_http_client.h>
#include <esp_log.h>
#include <cJSON.h>
#include <string.h>
#include <stdio.h>
#include "protocol.h"
#include "device_config.h"
#include "sdkconfig.h"

static const char *TAG = "unraid_client";

// Use Kconfig value if available, else default (user should configure this in menuconfig)
#ifdef CONFIG_UNRAID_API_URL
    #define UNRAID_API_URL CONFIG_UNRAID_API_URL
#else
    #define UNRAID_API_URL "http://192.168.1.100:8000/logs/ingest"
#endif

// Event handler for HTTP client
static esp_err_t http_event_handle(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGW(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER");
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA");
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        default:
            break;
    }
    return ESP_OK;
}

void send_log_to_unraid(mesh_message_t *msg) {
    if (!msg) {
        return;
    }

    // Get device config for network context
    const device_config_t *config = device_config_get();

    // 1. Serialize message to JSON matching LogIngestRequest
    cJSON *root = cJSON_CreateObject();
    cJSON *logs = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "logs", logs);

    cJSON *item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "device_id", msg->device_id);
    cJSON_AddNumberToObject(item, "timestamp", (double)msg->timestamp);
    
    // Parse message type to determine level and category
    const char *level = "INFO";
    const char *category = "system";
    
    if (msg->type == MSG_TYPE_MOTION) {
        level = "NOTICE";
        category = "motion";
    } else if (msg->type == MSG_TYPE_LOG) {
        level = "INFO";
        category = "system";
    }
    
    cJSON_AddStringToObject(item, "level", level);
    cJSON_AddStringToObject(item, "category", category);
    cJSON_AddStringToObject(item, "message", msg->payload);

    // Hex encode the signature (64 bytes = 128 hex chars)
    char signature_hex[129];
    for (int i = 0; i < 64; i++) {
        sprintf(&signature_hex[i*2], "%02x", msg->signature[i]);
    }
    signature_hex[128] = '\0';
    cJSON_AddStringToObject(item, "signature", signature_hex);

    cJSON_AddItemToArray(logs, item);

    char *json_str = cJSON_PrintUnformatted(root);
    if (!json_str) {
        ESP_LOGE(TAG, "Failed to serialize JSON");
        cJSON_Delete(root);
        return;
    }

    // 2. Create HTTP request
    esp_http_client_config_t http_config = {
        .url = UNRAID_API_URL,
        .event_handler = http_event_handle,
        .transport_type = HTTP_TRANSPORT_OVER_TCP,
        .timeout_ms = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&http_config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to create HTTP client");
        free(json_str);
        cJSON_Delete(root);
        return;
    }

    // 3. Send POST request
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_str, strlen(json_str));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "Log sent to Unraid: %d bytes, response: %d", strlen(json_str), status_code);
    } else {
        ESP_LOGE(TAG, "Failed to send log to Unraid: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    free(json_str);
    cJSON_Delete(root);
}

// Batch logging function (for future use with message buffering)
void send_log_batch_to_unraid(cJSON *logs_array) {
    if (!logs_array) {
        return;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "logs", logs_array);

    char *json_str = cJSON_PrintUnformatted(root);
    if (!json_str) {
        ESP_LOGE(TAG, "Failed to serialize batch JSON");
        cJSON_Delete(root);
        return;
    }

    esp_http_client_config_t http_config = {
        .url = UNRAID_API_URL,
        .event_handler = http_event_handle,
        .transport_type = HTTP_TRANSPORT_OVER_TCP,
        .timeout_ms = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&http_config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to create HTTP client");
        free(json_str);
        cJSON_Delete(root);
        return;
    }

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_str, strlen(json_str));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "Log batch sent to Unraid: response code %d", status_code);
    } else {
        ESP_LOGE(TAG, "Failed to send log batch: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    free(json_str);
    cJSON_Delete(root);
}
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Log sent, status = %d", esp_http_client_get_status_code(client));
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    cJSON_Delete(root);
    free(json_str);
}
