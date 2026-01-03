# Device Config Portal - Firmware Integration Guide

This guide shows how to integrate the Device Config Portal into your ESP32 firmware.

## Quick Start (5 minutes)

### Step 1: Copy Portal to Firmware

```bash
cd home_base_firmware/main/spiffs_image
# Copy the portal HTML file
cp ../../../device_config_portal/index.html .
```

### Step 2: Ensure SPIFFS is Built

Edit `home_base_firmware/CMakeLists.txt` to include SPIFFS:

```cmake
# In CMakeLists.txt
spiffs_create_partition_image(spiffs
    main/spiffs_image
    FLASH_IN_PROJECT
)
```

### Step 3: Implement HTTP Server Handlers

Add these handlers to your HTTP server code (in `http_server.c`):

```c
// Serve the portal index.html
static esp_err_t http_get_handler(httpd_req_t *req) {
    // Serve the device config portal from SPIFFS
    FILE *f = fopen("/spiffs/index.html", "r");
    if (!f) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Portal not found");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    
    char buffer[1024];
    size_t read_bytes;
    while ((read_bytes = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        httpd_resp_send_chunk(req, buffer, read_bytes);
    }
    
    httpd_resp_send_chunk(req, NULL, 0);
    fclose(f);
    return ESP_OK;
}

// WiFi scan endpoint
static esp_err_t http_wifi_scan_handler(httpd_req_t *req) {
    uint16_t number = 0;
    wifi_ap_record_t ap_info[20];
    
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
    
    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    
    // Build JSON response
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "[");
    
    for (int i = 0; i < number; i++) {
        char json_line[256];
        snprintf(json_line, sizeof(json_line),
            "%s{\"ssid\":\"%s\",\"rssi\":%d,\"security\":%d}",
            i > 0 ? "," : "",
            (char *)ap_info[i].ssid,
            ap_info[i].rssi,
            ap_info[i].authmode);
        httpd_resp_sendstr_chunk(req, json_line);
    }
    
    httpd_resp_sendstr(req, "]");
    return ESP_OK;
}

// Config save endpoint
static esp_err_t http_config_save_handler(httpd_req_t *req) {
    char content[2048] = {0};
    int total_len = req->content_len;
    int cur_len = 0;
    int received = 0;
    
    if (total_len >= sizeof(content)) {
        httpd_resp_send_err(req, HTTPD_413_PAYLOAD_TOO_LARGE, "Content too large");
        return ESP_FAIL;
    }
    
    // Read request body
    while (cur_len < total_len) {
        received = httpd_req_recv(req, content + cur_len, total_len - cur_len);
        if (received <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read body");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    content[cur_len] = '\0';
    
    // Parse JSON (using cJSON or manual parsing)
    // For simplicity, we'll use cJSON here
    cJSON *root = cJSON_Parse(content);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    // Extract fields
    const char *device_id = cJSON_GetStringValue(cJSON_GetObjectItem(root, "device_id"));
    int network_id = cJSON_GetObjectItem(root, "network_id")->valueint;
    const char *type = cJSON_GetStringValue(cJSON_GetObjectItem(root, "type"));
    const char *private_key = cJSON_GetStringValue(cJSON_GetObjectItem(root, "private_key"));
    int pir_gpio = cJSON_GetObjectItem(root, "pir_gpio")->valueint;
    int pir_sensitivity = cJSON_GetObjectItem(root, "pir_sensitivity")->valueint;
    int pir_cooldown_ms = cJSON_GetObjectItem(root, "pir_cooldown_ms")->valueint;
    int led_gpio = cJSON_GetObjectItem(root, "led_gpio")->valueint;
    int led_brightness = cJSON_GetObjectItem(root, "led_brightness")->valueint;
    
    // Build NVS config JSON
    cJSON *config = cJSON_CreateObject();
    cJSON_AddStringToObject(config, "device_id", device_id);
    cJSON_AddNumberToObject(config, "network_id", network_id);
    cJSON_AddStringToObject(config, "type", type);
    cJSON_AddStringToObject(config, "private_key", private_key);
    cJSON_AddNumberToObject(config, "pir_gpio", pir_gpio);
    cJSON_AddNumberToObject(config, "pir_sensitivity", pir_sensitivity);
    cJSON_AddNumberToObject(config, "pir_cooldown_ms", pir_cooldown_ms);
    cJSON_AddNumberToObject(config, "led_gpio", led_gpio);
    cJSON_AddNumberToObject(config, "led_brightness", led_brightness);
    
    char *config_str = cJSON_Print(config);
    
    // Save to NVS
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("device", NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        nvs_set_str(nvs_handle, "config", config_str);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
    }
    
    // Cleanup
    free(config_str);
    cJSON_Delete(config);
    cJSON_Delete(root);
    
    // Response
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"saved\"}");
    
    // Schedule reboot (give HTTP response time to send)
    // Use a background task to avoid hanging the request
    
    return ESP_OK;
}

// Reboot endpoint
static esp_err_t http_reboot_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"rebooting\"}");
    
    // Schedule reboot for a few seconds from now
    esp_err_t err = esp_register_shutdown_handler((shutdown_handler_t)esp_restart);
    
    return ESP_OK;
}
```

### Step 4: Register HTTP Routes

In your HTTP server initialization:

```c
void start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 20;
    config.max_open_sockets = 10;
    
    httpd_handle_t server = NULL;
    
    if (httpd_start(&server, &config) == ESP_OK) {
        // Register URI handlers
        
        // Root handler (serve portal)
        httpd_uri_t root_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = http_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &root_uri);
        
        // WiFi scan
        httpd_uri_t wifi_scan_uri = {
            .uri = "/api/wifi/scan",
            .method = HTTP_GET,
            .handler = http_wifi_scan_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &wifi_scan_uri);
        
        // Config save
        httpd_uri_t config_save_uri = {
            .uri = "/api/config/save",
            .method = HTTP_POST,
            .handler = http_config_save_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &config_save_uri);
        
        // Reboot
        httpd_uri_t reboot_uri = {
            .uri = "/api/reboot",
            .method = HTTP_POST,
            .handler = http_reboot_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &reboot_uri);
        
        ESP_LOGI(TAG, "Web server started");
    }
}
```

## AP Mode Activation Logic

Add this to your main initialization:

```c
// In app_main()
void app_main(void) {
    // ... existing initialization ...
    
    // Check if device is configured
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("device", NVS_READONLY, &nvs_handle);
    
    bool is_configured = false;
    if (err == ESP_OK) {
        char config_str[256];
        size_t len = sizeof(config_str);
        err = nvs_get_str(nvs_handle, "config", config_str, &len);
        is_configured = (err == ESP_OK);
        nvs_close(nvs_handle);
    }
    
    if (!is_configured) {
        // Device not configured - start AP mode with portal
        ESP_LOGI(TAG, "Device not configured. Starting AP for setup...");
        start_wifi_ap_mode();
    } else {
        // Device configured - connect to WiFi
        start_wifi_sta_mode();
    }
    
    // Start web server (serves portal in AP mode)
    start_webserver();
    
    // Rest of initialization...
}

void start_wifi_ap_mode(void) {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "ESP32-MESH-",
            .ssid_len = 0, // Auto-fill with MAC
            .password = "",
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN
        },
    };
    
    // Append MAC address to SSID
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_AP, mac);
    snprintf((char*)wifi_config.ap.ssid + strlen("ESP32-MESH-"), 
             sizeof(wifi_config.ap.ssid) - strlen("ESP32-MESH-"),
             "%02X%02X%02X", mac[3], mac[4], mac[5]);
    wifi_config.ap.ssid_len = strlen((char*)wifi_config.ap.ssid);
    
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    esp_wifi_start();
    
    ESP_LOGI(TAG, "AP Mode Started: %s", wifi_config.ap.ssid);
}
```

## Configuration File Structure (NVS)

After user completes setup, configuration is stored in NVS:

**Namespace:** `device`  
**Key:** `config`  
**Type:** String (JSON)

**Example content:**
```json
{
  "device_id": "ESP32-ABC123",
  "network_id": 1,
  "type": "motion",
  "private_key": "a1b2c3d4e5f6...",
  "pir_gpio": 39,
  "pir_sensitivity": 5,
  "pir_cooldown_ms": 30000,
  "led_gpio": 48,
  "led_brightness": 80,
  "board_variant": "esp32s3_devkitm"
}
```

## Loading Configuration at Boot

```c
// Add to initialization
void load_device_config(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("device", NVS_READONLY, &nvs_handle);
    
    if (err == ESP_OK) {
        char config_str[512];
        size_t len = sizeof(config_str);
        
        if (nvs_get_str(nvs_handle, "config", config_str, &len) == ESP_OK) {
            cJSON *config = cJSON_Parse(config_str);
            if (config) {
                // Load into global config struct
                g_device_config.network_id = 
                    cJSON_GetNumberValue(cJSON_GetObjectItem(config, "network_id"));
                strcpy(g_device_config.device_id,
                    cJSON_GetStringValue(cJSON_GetObjectItem(config, "device_id")));
                // ... load other fields
                
                cJSON_Delete(config);
            }
        }
        nvs_close(nvs_handle);
    }
}
```

## Testing the Integration

### 1. Build and Flash
```bash
cd home_base_firmware
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

### 2. Connect to Device AP
```bash
# The device should appear as "ESP32-MESH-XXXXXX"
# Connect from your phone or computer
# Open browser and navigate to: http://192.168.4.1
```

### 3. Complete Setup Wizard
- Select device type
- Scan WiFi (should see your networks)
- Enter WiFi password
- Fill in Unraid network details (Network ID, Device ID, Private Key)
- Configure sensors
- Review and save

### 4. Verify Configuration
After reboot, check the device logs:
```bash
idf.py monitor

# Should see:
# [device_config] Loading configuration from NVS...
# [device_config] Device ID: ESP32-ABC123
# [device_config] Network: 1
```

## Troubleshooting

**Issue: "Portal not found" error**
- Solution: Ensure SPIFFS image is built and flashed
- Check: `idf.py partition_table` shows `spiffs` partition

**Issue: WiFi scan returns empty**
- Solution: WiFi must be initialized before HTTP server
- Check: Call `init_wifi_ap()` before `start_webserver()`

**Issue: Configuration not saving**
- Solution: Ensure NVS is initialized and has space
- Check: `idf.py nvs-flash-erase` to reset NVS

**Issue: Device not rebooting after save**
- Solution: Reboot is asynchronous, may take 2-3 seconds
- Check: Power cycle if stuck, then restart

## Size Budget Analysis

- Portal HTML: ~45KB
- SPIFFS partition (recommended): 256KB - 512KB
- Configuration in NVS: ~1KB per device
- **Total overhead: negligible**

## Performance Notes

- WiFi scan takes ~2-3 seconds (depends on channel count)
- Configuration save is immediate
- Reboot takes ~1 second after response sent
- HTTP requests should complete in <1 second

## Next Steps

After user completes device setup:

1. **Device joins mesh** - Connects to home base via ESP-NOW
2. **Home Base logs it** - Device appears in Home Base Dashboard
3. **User can manage** - Via Home Base Dashboard or Unraid Central
4. **OTA updates** - Can push firmware updates from Unraid

