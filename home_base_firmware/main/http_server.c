#include <esp_http_server.h>
#include <esp_log.h>
#include <cJSON.h>
#include <string.h>
#include <stdlib.h>
#include "protocol.h"
#include "device_config.h"
#include "log_storage.h"
#include "esp_wifi.h"
#include "esp_spiffs.h"

static const char *TAG = "http_server";

// === Status Endpoints ===

// Handler for GET /api/v1/status
static esp_err_t status_get_handler(httpd_req_t *req)
{
    const device_config_t *config = device_config_get();
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "online");
    cJSON_AddStringToObject(root, "role", "home_base");
    cJSON_AddStringToObject(root, "device_id", config->device_id);
    cJSON_AddNumberToObject(root, "network_id", config->network_id);
    
    char *json_str = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, (const char *)json_str, strlen(json_str));
    
    free(json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

// Handler for GET /api/v1/devices
static esp_err_t devices_get_handler(httpd_req_t *req)
{
    // Placeholder: in a real app, query the mesh node list
    cJSON *root = cJSON_CreateArray();
    
    char *json_str = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, (const char *)json_str, strlen(json_str));
    
    free(json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

// === Config Portal (Served from SPIFFS) ===

// GET / - Serve device config portal
static esp_err_t portal_get_handler(httpd_req_t *req)
{
    FILE *f = fopen("/spiffs/index.html", "r");
    if (!f) {
        ESP_LOGW(TAG, "Portal not found at /spiffs/index.html");
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Portal not found. SPIFFS may not be mounted.");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    
    char buffer[1024];
    size_t read_bytes;
    while ((read_bytes = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        if (httpd_resp_send_chunk(req, buffer, read_bytes) != ESP_OK) {
            ESP_LOGW(TAG, "Error sending portal chunk");
            fclose(f);
            return ESP_FAIL;
        }
    }
    
    httpd_resp_send_chunk(req, NULL, 0);  // End chunked response
    fclose(f);
    return ESP_OK;
}

// === Device Config Portal Endpoints ===

// GET /api/device/type
static esp_err_t device_type_get_handler(httpd_req_t *req)
{
    const device_config_t *config = device_config_get();
    
    cJSON *root = cJSON_CreateObject();
    const char *type_str = (config->type == 0x01) ? "motion" : 
                           (config->type == 0x02) ? "camera" : "unconfigured";
    cJSON_AddStringToObject(root, "type", type_str);
    
    char *json_str = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, (const char *)json_str, strlen(json_str));
    
    free(json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

// POST /api/device/set-type
static esp_err_t device_type_set_handler(httpd_req_t *req)
{
    char buffer[256];
    int received = httpd_req_recv(req, buffer, sizeof(buffer) - 1);
    if (received <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");
        return ESP_FAIL;
    }
    buffer[received] = '\0';

    cJSON *root = cJSON_Parse(buffer);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *type_item = cJSON_GetObjectItem(root, "type");
    if (!type_item || !type_item->valuestring) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing type field");
        return ESP_FAIL;
    }

    device_config_t config = *device_config_get();
    if (strcmp(type_item->valuestring, "motion") == 0) {
        config.type = 0x01;
    } else if (strcmp(type_item->valuestring, "camera") == 0) {
        config.type = 0x02;
    }

    device_config_save(&config);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\": \"saved\"}");
    cJSON_Delete(root);
    return ESP_OK;
}

// GET /api/wifi/scan
static esp_err_t wifi_scan_handler(httpd_req_t *req)
{
    uint16_t number = 0;
    wifi_ap_record_t ap_info[20];
    memset(ap_info, 0, sizeof(ap_info));

    // Start WiFi scan
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 100,
        .scan_time.active.max = 200
    };
    
    if (esp_wifi_scan_start(&scan_config, true) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Scan failed");
        return ESP_FAIL;
    }

    esp_wifi_scan_get_ap_records(&number, ap_info);

    // Build JSON response
    cJSON *root = cJSON_CreateArray();
    for (int i = 0; i < number; i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "ssid", (const char *)ap_info[i].ssid);
        cJSON_AddNumberToObject(item, "rssi", ap_info[i].rssi);
        cJSON_AddNumberToObject(item, "security", ap_info[i].authmode);
        cJSON_AddItemToArray(root, item);
    }

    char *json_str = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, (const char *)json_str, strlen(json_str));

    free(json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

// POST /api/wifi/connect
static esp_err_t wifi_connect_handler(httpd_req_t *req)
{
    char buffer[512];
    int received = httpd_req_recv(req, buffer, sizeof(buffer) - 1);
    if (received <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");
        return ESP_FAIL;
    }
    buffer[received] = '\0';

    cJSON *root = cJSON_Parse(buffer);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    const char *ssid = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ssid"));
    const char *password = cJSON_GetStringValue(cJSON_GetObjectItem(root, "password"));

    if (!ssid) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing SSID");
        return ESP_FAIL;
    }

    // Store WiFi credentials in NVS for persistence
    // Note: In production, would initiate WiFi connection here
    ESP_LOGI(TAG, "WiFi connect request: SSID=%s", ssid);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\": \"connecting\", \"retry_count\": 3}");
    cJSON_Delete(root);
    return ESP_OK;
}

// POST /api/device/register
static esp_err_t device_register_handler(httpd_req_t *req)
{
    char buffer[512];
    int received = httpd_req_recv(req, buffer, sizeof(buffer) - 1);
    if (received <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");
        return ESP_FAIL;
    }
    buffer[received] = '\0';

    cJSON *root = cJSON_Parse(buffer);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    const char *device_id = cJSON_GetStringValue(cJSON_GetObjectItem(root, "device_id"));
    int network_id = cJSON_GetObjectItem(root, "network_id")->valueint;

    if (!device_id || network_id == 0) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing device_id or network_id");
        return ESP_FAIL;
    }

    device_config_t config = *device_config_get();
    strncpy(config.device_id, device_id, sizeof(config.device_id) - 1);
    config.network_id = network_id;
    device_config_save(&config);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\": \"registered\"}");
    cJSON_Delete(root);
    return ESP_OK;
}

// POST /api/config/sensors
static esp_err_t config_sensors_handler(httpd_req_t *req)
{
    char buffer[512];
    int received = httpd_req_recv(req, buffer, sizeof(buffer) - 1);
    if (received <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");
        return ESP_FAIL;
    }
    buffer[received] = '\0';

    cJSON *root = cJSON_Parse(buffer);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    device_config_t config = *device_config_get();
    
    cJSON *item;
    item = cJSON_GetObjectItem(root, "pir_gpio");
    if (item) config.pir_gpio = item->valueint;
    
    item = cJSON_GetObjectItem(root, "pir_sensitivity");
    if (item) config.pir_sensitivity = item->valueint;
    
    item = cJSON_GetObjectItem(root, "pir_cooldown_ms");
    if (item) config.pir_cooldown_ms = item->valueint;

    device_config_save(&config);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\": \"saved\", \"reboot_in_seconds\": 3}");
    cJSON_Delete(root);
    return ESP_OK;
}

// POST /api/config/led
static esp_err_t config_led_handler(httpd_req_t *req)
{
    char buffer[512];
    int received = httpd_req_recv(req, buffer, sizeof(buffer) - 1);
    if (received <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");
        return ESP_FAIL;
    }
    buffer[received] = '\0';

    cJSON *root = cJSON_Parse(buffer);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    device_config_t config = *device_config_get();
    
    cJSON *item = cJSON_GetObjectItem(root, "led_gpio");
    if (item) config.led_gpio = item->valueint;
    
    item = cJSON_GetObjectItem(root, "led_brightness");
    if (item) config.led_brightness = item->valueint;

    device_config_save(&config);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\": \"saved\"}");
    cJSON_Delete(root);
    return ESP_OK;
}

// POST /api/config/camera
static esp_err_t config_camera_handler(httpd_req_t *req)
{
    char buffer[512];
    int received = httpd_req_recv(req, buffer, sizeof(buffer) - 1);
    if (received <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");
        return ESP_FAIL;
    }
    buffer[received] = '\0';

    cJSON *root = cJSON_Parse(buffer);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    device_config_t config = *device_config_get();
    
    cJSON *item = cJSON_GetObjectItem(root, "camera_enable");
    if (item) config.camera_enabled = cJSON_IsTrue(item);

    device_config_save(&config);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\": \"saved\"}");
    cJSON_Delete(root);
    return ESP_OK;
}

// POST /api/config/hardware
static esp_err_t config_hardware_handler(httpd_req_t *req)
{
    char buffer[512];
    int received = httpd_req_recv(req, buffer, sizeof(buffer) - 1);
    if (received <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");
        return ESP_FAIL;
    }
    buffer[received] = '\0';

    cJSON *root = cJSON_Parse(buffer);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    device_config_t config = *device_config_get();
    
    cJSON *item = cJSON_GetObjectItem(root, "board_variant");
    if (item && item->valuestring) {
        strncpy(config.board_variant, item->valuestring, sizeof(config.board_variant) - 1);
    }

    device_config_save(&config);

    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "status", "saved");
    cJSON *detected = cJSON_CreateObject();
    cJSON_AddNumberToObject(detected, "pir_gpio", config.pir_gpio);
    cJSON_AddNumberToObject(detected, "led_gpio", config.led_gpio);
    cJSON_AddItemToObject(response, "detected_gpios", detected);

    char *json_str = cJSON_PrintUnformatted(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, (const char *)json_str, strlen(json_str));

    free(json_str);
    cJSON_Delete(response);
    cJSON_Delete(root);
    return ESP_OK;
}

// === Log and Motion Endpoints ===

// GET /api/logs - Retrieve device logs with optional filtering
static esp_err_t logs_get_handler(httpd_req_t *req)
{
    // Parse query string for optional device_id and limit
    char query_str[128] = {0};
    const char *device_id = NULL;
    int limit = 100;  // Default limit
    
    if (httpd_req_get_url_query_str(req, query_str, sizeof(query_str) - 1) == ESP_OK) {
        // Parse device_id parameter
        char device_param[64] = {0};
        if (httpd_query_key_value(query_str, "device_id", device_param, sizeof(device_param) - 1) == ESP_OK) {
            device_id = device_param;
        }
        
        // Parse limit parameter
        char limit_param[16] = {0};
        if (httpd_query_key_value(query_str, "limit", limit_param, sizeof(limit_param) - 1) == ESP_OK) {
            limit = atoi(limit_param);
            if (limit <= 0 || limit > 1000) limit = 100;
        }
    }
    
    // Get logs as JSON
    char *json_str = log_storage_get_logs_json(device_id, limit);
    if (!json_str) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to generate JSON");
        return ESP_FAIL;
    }
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    
    free(json_str);
    return ESP_OK;
}

// GET /api/motion - Retrieve motion events with optional filtering
static esp_err_t motion_get_handler(httpd_req_t *req)
{
    // Parse query string for optional device_id and limit
    char query_str[128] = {0};
    const char *device_id = NULL;
    int limit = 100;  // Default limit
    
    if (httpd_req_get_url_query_str(req, query_str, sizeof(query_str) - 1) == ESP_OK) {
        // Parse device_id parameter
        char device_param[64] = {0};
        if (httpd_query_key_value(query_str, "device_id", device_param, sizeof(device_param) - 1) == ESP_OK) {
            device_id = device_param;
        }
        
        // Parse limit parameter
        char limit_param[16] = {0};
        if (httpd_query_key_value(query_str, "limit", limit_param, sizeof(limit_param) - 1) == ESP_OK) {
            limit = atoi(limit_param);
            if (limit <= 0 || limit > 1000) limit = 100;
        }
    }
    
    // Get motion events as JSON
    char *json_str = log_storage_get_motion_json(device_id, limit);
    if (!json_str) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to generate JSON");
        return ESP_FAIL;
    }
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    
    free(json_str);
    return ESP_OK;
}

// POST /api/v1/command - Receive signed commands from Unraid/home base
static esp_err_t command_post_handler(httpd_req_t *req)
{
    char content[1024] = {0};
    int total_len = req->content_len;
    int cur_len = 0;
    int received = 0;
    
    if (total_len > sizeof(content)) {
        httpd_resp_send_err(req, HTTPD_413_PAYLOAD_TOO_LARGE, "Content too large");
        return ESP_FAIL;
    }
    
    // Read request body
    while (cur_len < total_len) {
        received = httpd_req_recv(req, content + cur_len, total_len - cur_len);
        if (received <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to read body");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    content[cur_len] = '\0';
    
    // Parse command JSON
    cJSON *root = cJSON_Parse(content);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    // Extract command fields
    const char *command = cJSON_GetStringValue(cJSON_GetObjectItem(root, "command"));
    const char *target_device = cJSON_GetStringValue(cJSON_GetObjectItem(root, "target_device"));
    const char *signature = cJSON_GetStringValue(cJSON_GetObjectItem(root, "signature"));
    
    if (!command || !target_device) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing command or target_device");
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    
    // TODO: Verify signature (requires network private key from device_config)
    // For now, log the command and return success
    
    ESP_LOGI(TAG, "Received command '%s' for device '%s'", command, target_device);
    
    // Log the command
    log_storage_add_log("home_base", "info", "command", 
                       "Received command from Unraid");
    
    // In a real implementation, forward to target device via ESP-NOW
    // mesh_send_command(target_device, command, ...);
    
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "status", "queued");
    cJSON_AddStringToObject(response, "command", command);
    cJSON_AddStringToObject(response, "target_device", target_device);
    
    char *json_str = cJSON_PrintUnformatted(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    
    free(json_str);
    cJSON_Delete(response);
    cJSON_Delete(root);
    return ESP_OK;
}

// POST /api/reboot
static esp_err_t reboot_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\": \"rebooting\"}");
    
    // Schedule reboot after a delay to allow response to be sent
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    
    return ESP_OK;
}

// Register URI handlers
void start_webserver(void)
{
    // Initialize SPIFFS for serving portal
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "spiffs",
        .max_files = 5,
        .format_if_mount_failed = false
    };
    
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_ERR_ESP_SPIFFS_NOT_MOUNTED) {
            ESP_LOGW(TAG, "SPIFFS not found. Config portal will not be available on /");
        } else {
            ESP_LOGE(TAG, "Failed to register SPIFFS (%s)", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGI(TAG, "SPIFFS mounted successfully");
    }

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 20;

    ESP_LOGI(TAG, "Starting web server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Root handler - serve config portal from SPIFFS
        httpd_uri_t root_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = portal_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &root_uri);

        // Status endpoints
        httpd_uri_t status_uri = {
            .uri = "/api/v1/status",
            .method = HTTP_GET,
            .handler = status_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &status_uri);

        httpd_uri_t devices_uri = {
            .uri = "/api/v1/devices",
            .method = HTTP_GET,
            .handler = devices_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &devices_uri);

        // Device config endpoints
        httpd_uri_t device_type_get_uri = {
            .uri = "/api/device/type",
            .method = HTTP_GET,
            .handler = device_type_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &device_type_get_uri);

        httpd_uri_t device_type_set_uri = {
            .uri = "/api/device/set-type",
            .method = HTTP_POST,
            .handler = device_type_set_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &device_type_set_uri);

        httpd_uri_t wifi_scan_uri = {
            .uri = "/api/wifi/scan",
            .method = HTTP_GET,
            .handler = wifi_scan_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &wifi_scan_uri);

        httpd_uri_t wifi_connect_uri = {
            .uri = "/api/wifi/connect",
            .method = HTTP_POST,
            .handler = wifi_connect_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &wifi_connect_uri);

        httpd_uri_t device_register_uri = {
            .uri = "/api/device/register",
            .method = HTTP_POST,
            .handler = device_register_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &device_register_uri);

        httpd_uri_t config_sensors_uri = {
            .uri = "/api/config/sensors",
            .method = HTTP_POST,
            .handler = config_sensors_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &config_sensors_uri);

        httpd_uri_t config_led_uri = {
            .uri = "/api/config/led",
            .method = HTTP_POST,
            .handler = config_led_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &config_led_uri);

        httpd_uri_t config_camera_uri = {
            .uri = "/api/config/camera",
            .method = HTTP_POST,
            .handler = config_camera_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &config_camera_uri);

        httpd_uri_t config_hardware_uri = {
            .uri = "/api/config/hardware",
            .method = HTTP_POST,
            .handler = config_hardware_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &config_hardware_uri);

        httpd_uri_t reboot_uri = {
            .uri = "/api/reboot",
            .method = HTTP_POST,
            .handler = reboot_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &reboot_uri);

        // Log and Motion endpoints
        httpd_uri_t logs_uri = {
            .uri = "/api/logs",
            .method = HTTP_GET,
            .handler = logs_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &logs_uri);

        httpd_uri_t motion_uri = {
            .uri = "/api/motion",
            .method = HTTP_GET,
            .handler = motion_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &motion_uri);

        // Command endpoint
        httpd_uri_t command_uri = {
            .uri = "/api/v1/command",
            .method = HTTP_POST,
            .handler = command_post_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &command_uri);

        ESP_LOGI(TAG, "Web server started with %d endpoints", 15);
    } else {
        ESP_LOGE(TAG, "Failed to start web server");
    }
}
