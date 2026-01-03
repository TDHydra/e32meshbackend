#include <stdio.h>
#include <string.h>
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "device_config.h"
#include "http_server.h"

static const char *TAG = "http_server";
static httpd_handle_t server = NULL;

// Forward declare handlers
static esp_err_t handler_wifi_scan(httpd_req_t *req);
static esp_err_t handler_config_motion(httpd_req_t *req);
static esp_err_t handler_config_display(httpd_req_t *req);
static esp_err_t handler_device_register(httpd_req_t *req);
static esp_err_t handler_device_type(httpd_req_t *req);
static esp_err_t handler_reboot(httpd_req_t *req);
static esp_err_t handler_status(httpd_req_t *req);

/**
 * GET /api/wifi/scan - Return available WiFi networks
 */
static esp_err_t handler_wifi_scan(httpd_req_t *req)
{
    ESP_LOGI(TAG, "WiFi scan requested");
    
    // Start WiFi scan
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 100,
        .scan_time.active.max = 300
    };
    
    esp_wifi_scan_start(&scan_config, true);
    
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    
    if (ap_count == 0) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "[]");
        return ESP_OK;
    }
    
    wifi_ap_record_t ap_info[ap_count];
    esp_wifi_scan_get_ap_records(&ap_count, ap_info);
    
    // Build JSON response
    cJSON *root = cJSON_CreateArray();
    
    for (int i = 0; i < ap_count; i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "ssid", (const char *)ap_info[i].ssid);
        cJSON_AddNumberToObject(item, "rssi", ap_info[i].rssi);
        cJSON_AddNumberToObject(item, "security", ap_info[i].authmode);
        cJSON_AddNumberToObject(item, "channel", ap_info[i].primary);
        cJSON_AddItemToArray(root, item);
    }
    
    char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    
    free(json_str);
    cJSON_Delete(root);
    
    return ESP_OK;
}

/**
 * POST /api/config/motion - Update motion sensor configuration
 */
static esp_err_t handler_config_motion(httpd_req_t *req)
{
    char buf[512];
    int ret = httpd_req_recv(req, buf, sizeof(buf));
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to read body");
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    device_config_t config = *device_config_get();
    
    // Parse motion config
    cJSON *item = cJSON_GetObjectItem(root, "motion_gpio");
    if (item) config.motion_gpio = item->valueint;
    
    item = cJSON_GetObjectItem(root, "motion_sensitivity");
    if (item) config.motion_sensitivity = (item->valueint < 1 ? 1 : 
                                          (item->valueint > 10 ? 10 : item->valueint));
    
    item = cJSON_GetObjectItem(root, "motion_cooldown_ms");
    if (item) config.motion_cooldown_ms = item->valueint;
    
    // Save config
    esp_err_t err = device_config_save(&config);
    cJSON_Delete(root);
    
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save config");
        return ESP_FAIL;
    }
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"saved\"}");
    
    return ESP_OK;
}

/**
 * POST /api/config/display - Update display configuration
 */
static esp_err_t handler_config_display(httpd_req_t *req)
{
    char buf[1024];
    int ret = httpd_req_recv(req, buf, sizeof(buf));
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to read body");
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    device_config_t config = *device_config_get();
    
    // Parse display config
    cJSON *item = cJSON_GetObjectItem(root, "brightness");
    if (item) config.display_brightness = item->valueint;
    
    item = cJSON_GetObjectItem(root, "color_motion");
    if (item) config.color_motion = item->valueint;
    
    item = cJSON_GetObjectItem(root, "color_clear");
    if (item) config.color_clear = item->valueint;
    
    item = cJSON_GetObjectItem(root, "color_cooldown");
    if (item) config.color_cooldown = item->valueint;
    
    item = cJSON_GetObjectItem(root, "color_text");
    if (item) config.color_text = item->valueint;
    
    item = cJSON_GetObjectItem(root, "color_background");
    if (item) config.color_background = item->valueint;
    
    // Save config
    esp_err_t err = device_config_save(&config);
    cJSON_Delete(root);
    
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save config");
        return ESP_FAIL;
    }
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"saved\"}");
    
    return ESP_OK;
}

/**
 * POST /api/device/register - Register device with network
 */
static esp_err_t handler_device_register(httpd_req_t *req)
{
    char buf[512];
    int ret = httpd_req_recv(req, buf, sizeof(buf));
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to read body");
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    device_config_t config = *device_config_get();
    
    // Parse registration data
    cJSON *item = cJSON_GetObjectItem(root, "device_id");
    if (item && item->valuestring) {
        strncpy(config.device_id, item->valuestring, sizeof(config.device_id) - 1);
    }
    
    item = cJSON_GetObjectItem(root, "network_id");
    if (item) config.network_id = item->valueint;
    
    // Save config
    esp_err_t err = device_config_save(&config);
    cJSON_Delete(root);
    
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save config");
        return ESP_FAIL;
    }
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"registered\"}");
    
    return ESP_OK;
}

/**
 * GET /api/device/type - Get device type
 */
static esp_err_t handler_device_type(httpd_req_t *req)
{
    const device_config_t *config = device_config_get();
    
    char json[128];
    snprintf(json, sizeof(json), "{\"type\":%d,\"device_id\":\"%s\"}", 
             config->type, config->device_id);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);
    
    return ESP_OK;
}

/**
 * GET /api/v1/status - Device status
 */
static esp_err_t handler_status(httpd_req_t *req)
{
    const device_config_t *config = device_config_get();
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device_id", config->device_id);
    cJSON_AddNumberToObject(root, "network_id", config->network_id);
    cJSON_AddNumberToObject(root, "type", config->type);
    cJSON_AddBoolToObject(root, "configured", device_config_is_configured());
    cJSON_AddNumberToObject(root, "motion_gpio", config->motion_gpio);
    cJSON_AddNumberToObject(root, "motion_sensitivity", config->motion_sensitivity);
    cJSON_AddNumberToObject(root, "display_brightness", config->display_brightness);
    
    char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    
    free(json_str);
    cJSON_Delete(root);
    
    return ESP_OK;
}

/**
 * POST /api/reboot - Reboot device
 */
static esp_err_t handler_reboot(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"rebooting\"}");
    
    // Schedule reboot for a few seconds from now
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
    
    return ESP_OK;
}

esp_err_t http_server_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 20;
    config.task_priority = tskIDLE_PRIORITY + 1;
    
    ESP_LOGI(TAG, "Starting HTTP server on port %d", config.server_port);
    
    esp_err_t ret = httpd_start(&server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Register handlers
    httpd_uri_t wifi_scan_uri = {
        .uri = "/api/wifi/scan",
        .method = HTTP_GET,
        .handler = handler_wifi_scan,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &wifi_scan_uri);
    
    httpd_uri_t config_motion_uri = {
        .uri = "/api/config/motion",
        .method = HTTP_POST,
        .handler = handler_config_motion,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &config_motion_uri);
    
    httpd_uri_t config_display_uri = {
        .uri = "/api/config/display",
        .method = HTTP_POST,
        .handler = handler_config_display,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &config_display_uri);
    
    httpd_uri_t device_register_uri = {
        .uri = "/api/device/register",
        .method = HTTP_POST,
        .handler = handler_device_register,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &device_register_uri);
    
    httpd_uri_t device_type_uri = {
        .uri = "/api/device/type",
        .method = HTTP_GET,
        .handler = handler_device_type,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &device_type_uri);
    
    httpd_uri_t status_uri = {
        .uri = "/api/v1/status",
        .method = HTTP_GET,
        .handler = handler_status,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &status_uri);
    
    httpd_uri_t reboot_uri = {
        .uri = "/api/reboot",
        .method = HTTP_POST,
        .handler = handler_reboot,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &reboot_uri);
    
    ESP_LOGI(TAG, "HTTP server started with %d endpoints", 7);
    return ESP_OK;
}

void http_server_stop(void)
{
    if (server) {
        httpd_stop(server);
        server = NULL;
    }
}
