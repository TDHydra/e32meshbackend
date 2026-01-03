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

// Default configuration for ESP32-C6
static const device_config_t DEFAULT_CONFIG = {
    .device_id = "ESP32-C6-UNCONFIGURED",
    .network_id = 0,
    .type = 0x02,  // Camera/Motion type
    .home_base_mac = {0, 0, 0, 0, 0, 0},
    
    // Motion sensor defaults
    .motion_gpio = 4,
    .motion_sensitivity = 5,
    .motion_cooldown_ms = 30000,
    
    // Display defaults (ST7789 on SPI)
    .display_enabled = true,
    .display_width = 240,
    .display_height = 320,
    .display_sclk_gpio = 6,
    .display_mosi_gpio = 7,
    .display_miso_gpio = 5,
    .display_cs_gpio = 8,
    .display_dc_gpio = 9,
    .display_reset_gpio = 10,
    .display_backlight_gpio = 11,
    .display_brightness = 100,
    
    // Display colors (RGB565)
    .color_motion = 0xF800,      // Red
    .color_clear = 0x07E0,       // Green
    .color_cooldown = 0xFFE0,    // Yellow
    .color_text = 0xFFFF,        // White
    .color_background = 0x0000,  // Black
    
    .board_variant = "esp32c6",
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
    char config_str[1024];
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

        // Motion sensor config
        item = cJSON_GetObjectItem(root, "motion_gpio");
        if (item) g_device_config.motion_gpio = item->valueint;

        item = cJSON_GetObjectItem(root, "motion_sensitivity");
        if (item) g_device_config.motion_sensitivity = item->valueint;

        item = cJSON_GetObjectItem(root, "motion_cooldown_ms");
        if (item) g_device_config.motion_cooldown_ms = item->valueint;

        // Display config
        item = cJSON_GetObjectItem(root, "display_enabled");
        if (item) g_device_config.display_enabled = cJSON_IsTrue(item);

        item = cJSON_GetObjectItem(root, "display_brightness");
        if (item) g_device_config.display_brightness = item->valueint;

        item = cJSON_GetObjectItem(root, "display_sclk_gpio");
        if (item) g_device_config.display_sclk_gpio = item->valueint;

        item = cJSON_GetObjectItem(root, "display_mosi_gpio");
        if (item) g_device_config.display_mosi_gpio = item->valueint;

        item = cJSON_GetObjectItem(root, "display_miso_gpio");
        if (item) g_device_config.display_miso_gpio = item->valueint;

        item = cJSON_GetObjectItem(root, "display_cs_gpio");
        if (item) g_device_config.display_cs_gpio = item->valueint;

        item = cJSON_GetObjectItem(root, "display_dc_gpio");
        if (item) g_device_config.display_dc_gpio = item->valueint;

        item = cJSON_GetObjectItem(root, "display_reset_gpio");
        if (item) g_device_config.display_reset_gpio = item->valueint;

        item = cJSON_GetObjectItem(root, "display_backlight_gpio");
        if (item) g_device_config.display_backlight_gpio = item->valueint;

        // Colors (RGB565)
        item = cJSON_GetObjectItem(root, "color_motion");
        if (item) g_device_config.color_motion = item->valueint;

        item = cJSON_GetObjectItem(root, "color_clear");
        if (item) g_device_config.color_clear = item->valueint;

        item = cJSON_GetObjectItem(root, "color_cooldown");
        if (item) g_device_config.color_cooldown = item->valueint;

        item = cJSON_GetObjectItem(root, "color_text");
        if (item) g_device_config.color_text = item->valueint;

        item = cJSON_GetObjectItem(root, "color_background");
        if (item) g_device_config.color_background = item->valueint;

        cJSON_Delete(root);
        ESP_LOGI(TAG, "Loaded config: device_id=%s, motion_gpio=%d", 
                 g_device_config.device_id, g_device_config.motion_gpio);
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
    
    cJSON_AddNumberToObject(root, "motion_gpio", config->motion_gpio);
    cJSON_AddNumberToObject(root, "motion_sensitivity", config->motion_sensitivity);
    cJSON_AddNumberToObject(root, "motion_cooldown_ms", config->motion_cooldown_ms);
    
    cJSON_AddBoolToObject(root, "display_enabled", config->display_enabled);
    cJSON_AddNumberToObject(root, "display_brightness", config->display_brightness);
    cJSON_AddNumberToObject(root, "display_sclk_gpio", config->display_sclk_gpio);
    cJSON_AddNumberToObject(root, "display_mosi_gpio", config->display_mosi_gpio);
    cJSON_AddNumberToObject(root, "display_miso_gpio", config->display_miso_gpio);
    cJSON_AddNumberToObject(root, "display_cs_gpio", config->display_cs_gpio);
    cJSON_AddNumberToObject(root, "display_dc_gpio", config->display_dc_gpio);
    cJSON_AddNumberToObject(root, "display_reset_gpio", config->display_reset_gpio);
    cJSON_AddNumberToObject(root, "display_backlight_gpio", config->display_backlight_gpio);
    
    cJSON_AddNumberToObject(root, "color_motion", config->color_motion);
    cJSON_AddNumberToObject(root, "color_clear", config->color_clear);
    cJSON_AddNumberToObject(root, "color_cooldown", config->color_cooldown);
    cJSON_AddNumberToObject(root, "color_text", config->color_text);
    cJSON_AddNumberToObject(root, "color_background", config->color_background);
    
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

void device_config_update_motion_gpio(uint8_t gpio)
{
    device_config_t config = *device_config_get();
    config.motion_gpio = gpio;
    device_config_save(&config);
}

void device_config_update_display_colors(uint16_t motion, uint16_t clear, uint16_t cooldown, uint16_t text)
{
    device_config_t config = *device_config_get();
    config.color_motion = motion;
    config.color_clear = clear;
    config.color_cooldown = cooldown;
    config.color_text = text;
    device_config_save(&config);
}

void device_config_update_display_brightness(uint8_t brightness)
{
    device_config_t config = *device_config_get();
    config.display_brightness = (brightness > 100) ? 100 : brightness;
    device_config_save(&config);
}
