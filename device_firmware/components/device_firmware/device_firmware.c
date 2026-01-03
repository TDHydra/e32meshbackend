#include "device_firmware.h"
#include <esp_log.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_now.h>
#include <esp_random.h>
#include <time.h>
#include <cJSON.h>
#include <string.h>

static const char *TAG = "device_firmware";

// Global device configuration
static device_config_t g_device_config = {0};
static bool g_device_configured = false;
static nvs_handle_t g_nvs_handle = 0;

// WiFi event handler
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START) {
        ESP_LOGI(TAG, "WiFi AP Mode: Ready for configuration at 192.168.4.1");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi STA: Connecting...");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "WiFi STA: Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

/**
 * Load configuration from NVS
 */
static esp_err_t load_config(void)
{
    esp_err_t err = nvs_open("device", NVS_READONLY, &g_nvs_handle);
    if (err == ESP_ERR_NVS_NOT_INITIALIZED) {
        ESP_LOGI(TAG, "NVS not initialized, will use defaults");
        return ESP_OK;
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    // Try to load device_id (indicates device is configured)
    char device_id[32] = {0};
    size_t len = sizeof(device_id);
    err = nvs_get_str(g_nvs_handle, "device_id", device_id, &len);
    
    if (err == ESP_OK) {
        // Device is configured, load all settings
        strncpy(g_device_config.device_id, device_id, sizeof(g_device_config.device_id) - 1);
        
        // Load other config
        nvs_get_u32(g_nvs_handle, "network_id", &g_device_config.network_id);
        nvs_get_u8(g_nvs_handle, "device_type", &g_device_config.device_type);
        
        // Load PIR config
        nvs_get_u8(g_nvs_handle, "pir_gpio", &g_device_config.pir_gpio);
        nvs_get_u8(g_nvs_handle, "pir_sensitivity", &g_device_config.pir_sensitivity);
        nvs_get_u32(g_nvs_handle, "pir_cooldown_ms", &g_device_config.pir_cooldown_ms);
        
        // Load LED config
        nvs_get_u8(g_nvs_handle, "led_gpio", &g_device_config.led_gpio);
        nvs_get_u8(g_nvs_handle, "led_brightness", &g_device_config.led_brightness);
        
        // Load WiFi config
        char ssid[32] = {0};
        len = sizeof(ssid);
        if (nvs_get_str(g_nvs_handle, "ssid", ssid, &len) == ESP_OK) {
            strncpy(g_device_config.ssid, ssid, sizeof(g_device_config.ssid) - 1);
        }
        
        g_device_configured = true;
        ESP_LOGI(TAG, "Loaded config for device: %s (network_id=%d)",
                 g_device_config.device_id, g_device_config.network_id);
    } else {
        // Device not configured
        g_device_configured = false;
        ESP_LOGI(TAG, "No configuration found, device needs setup");
    }

    return ESP_OK;
}

/**
 * Initialize WiFi for AP or STA mode
 */
static void init_wifi(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
                                               &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, 
                                               &wifi_event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    if (g_device_configured) {
        // Start in STA mode
        device_start_sta_mode();
    } else {
        // Start in AP mode for configuration
        device_start_ap_mode();
    }
}

/**
 * Initialize ESP-NOW for mesh communication
 */
static void init_esp_now(void)
{
    // ESP-NOW requires WiFi to be initialized
    if (esp_now_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ESP-NOW");
        return;
    }

    // Register send callback (optional)
    // esp_now_register_send_cb(send_callback);

    ESP_LOGI(TAG, "ESP-NOW initialized");
}

// === Public API ===

void device_firmware_init(void)
{
    ESP_LOGI(TAG, "=== Device Firmware Start ===");

    // 1. Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Load configuration
    load_config();

    // 3. Initialize WiFi
    init_wifi();

    // 4. Initialize ESP-NOW (requires WiFi to be initialized)
    init_esp_now();

    ESP_LOGI(TAG, "Device Firmware Initialized Successfully");
}

void device_send_heartbeat(void)
{
    // Build heartbeat message
    cJSON *payload = cJSON_CreateObject();
    cJSON_AddNumberToObject(payload, "uptime_ms", (double)esp_timer_get_time() / 1000.0);
    cJSON_AddNumberToObject(payload, "heap_free", esp_get_free_heap_size());
    cJSON_AddNumberToObject(payload, "heap_min", esp_get_minimum_free_heap_size());

    char *payload_str = cJSON_PrintUnformatted(payload);
    cJSON_Delete(payload);

    // Send via ESP-NOW (TODO: implement message signing)
    // mesh_message_t msg = {...};
    // esp_now_send(home_base_mac, (uint8_t *)&msg, sizeof(msg));

    free(payload_str);
}

void device_send_motion_event(void)
{
    ESP_LOGI(TAG, "Motion detected!");

    // Build motion event message
    cJSON *payload = cJSON_CreateObject();
    cJSON_AddNumberToObject(payload, "intensity", 100);
    cJSON_AddNumberToObject(payload, "timestamp", (double)time(NULL));

    char *payload_str = cJSON_PrintUnformatted(payload);
    cJSON_Delete(payload);

    // Send via ESP-NOW (TODO: implement message signing)
    // mesh_message_t msg = {...};
    // esp_now_send(home_base_mac, (uint8_t *)&msg, sizeof(msg));

    free(payload_str);
}

void device_send_log(const char *level, const char *category, const char *message)
{
    // Build log message
    cJSON *payload = cJSON_CreateObject();
    cJSON_AddStringToObject(payload, "level", level);
    cJSON_AddStringToObject(payload, "category", category);
    cJSON_AddStringToObject(payload, "message", message);

    char *payload_str = cJSON_PrintUnformatted(payload);
    cJSON_Delete(payload);

    // Send via ESP-NOW (TODO: implement message signing)
    // mesh_message_t msg = {...};
    // esp_now_send(home_base_mac, (uint8_t *)&msg, sizeof(msg));

    free(payload_str);
}

const device_config_t* device_get_config(void)
{
    return &g_device_config;
}

bool device_is_configured(void)
{
    return g_device_configured;
}

void device_start_ap_mode(void)
{
    ESP_LOGI(TAG, "Starting WiFi AP Mode for configuration");

    // Create AP interface
    esp_netif_create_default_wifi_ap();

    wifi_config_t wifi_config = {
        .ap = {
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN,
            .channel = 1,
        },
    };

    // Set SSID to "ESP32-MESH-{MAC}"
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_AP, mac);
    snprintf((char *)wifi_config.ap.ssid, sizeof(wifi_config.ap.ssid),
             "ESP32-MESH-%02X%02X%02X", mac[3], mac[4], mac[5]);
    wifi_config.ap.ssid_len = strlen((char *)wifi_config.ap.ssid);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "AP started: SSID='%s'", wifi_config.ap.ssid);
}

void device_start_sta_mode(void)
{
    ESP_LOGI(TAG, "Starting WiFi STA Mode");

    // Create STA interface
    esp_netif_create_default_wifi_sta();

    wifi_config_t wifi_config = {
        .sta = {
            .scan_method = WIFI_FAST_SCAN,
            .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
            .threshold.rssi = -127,
            .threshold.authmode = WIFI_AUTH_OPEN,
        },
    };

    // Use stored SSID and password
    strncpy((char *)wifi_config.sta.ssid, g_device_config.ssid, 
            sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, g_device_config.password, 
            sizeof(wifi_config.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "STA mode started, connecting to '%s'", g_device_config.ssid);
}
