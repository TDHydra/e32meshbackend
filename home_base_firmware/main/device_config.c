#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include "device_config.h"

static const char *TAG = "device_config";

// Global device config
static device_config_t g_device_config = {0};
static bool g_config_loaded = false;

// Default configuration
static const device_config_t DEFAULT_CONFIG = {
    .device_id = "ESP32-P4-UNCONFIGURED",
    .network_id = 0,
    .type = 0,
    .pir_gpio = 39,
    .pir_sensitivity = 5,
    .pir_cooldown_ms = 30000,
    .led_gpio = 48,
    .led_brightness = 80,
    .camera_enabled = false,
    .board_variant = "esp32p4_eth",
};

esp_err_t device_config_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

esp_err_t device_config_load(void)
{
    if (g_config_loaded) {
        return ESP_OK;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("device", NVS_READONLY, &nvs_handle);
    
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS namespace not found, using defaults");
        memcpy(&g_device_config, &DEFAULT_CONFIG, sizeof(device_config_t));
        g_config_loaded = true;
        return ESP_OK;
    }

    // Try to read configuration JSON
    char config_str[512];
    size_t len = sizeof(config_str) - 1;
    err = nvs_get_str(nvs_handle, "config", config_str, &len);

    nvs_close(nvs_handle);

    if (err == ESP_OK) {
        // Parse JSON configuration
        cJSON *root = cJSON_Parse(config_str);
        if (!root) {
            ESP_LOGW(TAG, "Failed to parse config JSON, using defaults");
            memcpy(&g_device_config, &DEFAULT_CONFIG, sizeof(device_config_t));
            g_config_loaded = true;
            return ESP_OK;
        }

        // Extract fields from JSON
        cJSON *item;
        
        item = cJSON_GetObjectItem(root, "device_id");
        if (item && item->valuestring) {
            strncpy(g_device_config.device_id, item->valuestring, sizeof(g_device_config.device_id) - 1);
        }

        item = cJSON_GetObjectItem(root, "network_id");
        if (item) g_device_config.network_id = item->valueint;

        item = cJSON_GetObjectItem(root, "type");
        if (item) g_device_config.type = item->valueint;

        item = cJSON_GetObjectItem(root, "pir_gpio");
        if (item) g_device_config.pir_gpio = item->valueint;

        item = cJSON_GetObjectItem(root, "pir_sensitivity");
        if (item) g_device_config.pir_sensitivity = item->valueint;

        item = cJSON_GetObjectItem(root, "pir_cooldown_ms");
        if (item) g_device_config.pir_cooldown_ms = item->valueint;

        item = cJSON_GetObjectItem(root, "led_gpio");
        if (item) g_device_config.led_gpio = item->valueint;

        item = cJSON_GetObjectItem(root, "led_brightness");
        if (item) g_device_config.led_brightness = item->valueint;

        item = cJSON_GetObjectItem(root, "camera_enabled");
        if (item) g_device_config.camera_enabled = cJSON_IsTrue(item);

        item = cJSON_GetObjectItem(root, "board_variant");
        if (item && item->valuestring) {
            strncpy(g_device_config.board_variant, item->valuestring, sizeof(g_device_config.board_variant) - 1);
        }

        cJSON_Delete(root);
        ESP_LOGI(TAG, "Loaded config: device_id=%s, network_id=%d", 
                 g_device_config.device_id, g_device_config.network_id);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Config not found in NVS, using defaults");
        memcpy(&g_device_config, &DEFAULT_CONFIG, sizeof(device_config_t));
    } else {
        ESP_LOGE(TAG, "Error reading config from NVS: %s", esp_err_to_name(err));
        return err;
    }

    g_config_loaded = true;
    return ESP_OK;
}

esp_err_t device_config_save(const device_config_t *config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    // Create JSON representation
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device_id", config->device_id);
    cJSON_AddNumberToObject(root, "network_id", config->network_id);
    cJSON_AddNumberToObject(root, "type", config->type);
    cJSON_AddNumberToObject(root, "pir_gpio", config->pir_gpio);
    cJSON_AddNumberToObject(root, "pir_sensitivity", config->pir_sensitivity);
    cJSON_AddNumberToObject(root, "pir_cooldown_ms", config->pir_cooldown_ms);
    cJSON_AddNumberToObject(root, "led_gpio", config->led_gpio);
    cJSON_AddNumberToObject(root, "led_brightness", config->led_brightness);
    cJSON_AddBoolToObject(root, "camera_enabled", config->camera_enabled);
    cJSON_AddStringToObject(root, "board_variant", config->board_variant);

    char *config_str = cJSON_PrintUnformatted(root);
    if (!config_str) {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }

    // Save to NVS
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("device", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        free(config_str);
        cJSON_Delete(root);
        return err;
    }

    err = nvs_set_str(nvs_handle, "config", config_str);
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle);
        if (err == ESP_OK) {
            memcpy(&g_device_config, config, sizeof(device_config_t));
            ESP_LOGI(TAG, "Config saved: device_id=%s", config->device_id);
        }
    }

    nvs_close(nvs_handle);
    free(config_str);
    cJSON_Delete(root);
    
    return err;
}

const device_config_t* device_config_get(void)
{
    if (!g_config_loaded) {
        device_config_load();
    }
    return &g_device_config;
}

bool device_config_is_configured(void)
{
    if (!g_config_loaded) {
        device_config_load();
    }
    return g_device_config.network_id != 0;
}

char* device_config_to_json_string(void)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device_id", g_device_config.device_id);
    cJSON_AddNumberToObject(root, "network_id", g_device_config.network_id);
    cJSON_AddNumberToObject(root, "type", g_device_config.type);
    cJSON_AddNumberToObject(root, "pir_gpio", g_device_config.pir_gpio);
    cJSON_AddNumberToObject(root, "pir_sensitivity", g_device_config.pir_sensitivity);
    cJSON_AddNumberToObject(root, "pir_cooldown_ms", g_device_config.pir_cooldown_ms);
    cJSON_AddNumberToObject(root, "led_gpio", g_device_config.led_gpio);
    cJSON_AddNumberToObject(root, "led_brightness", g_device_config.led_brightness);
    cJSON_AddBoolToObject(root, "camera_enabled", g_device_config.camera_enabled);
    cJSON_AddStringToObject(root, "board_variant", g_device_config.board_variant);

    return cJSON_PrintUnformatted(root);
}
