#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "nvs_flash.h"
#include "protocol.h"
#include "device_config.h"
#include "log_storage.h"

// Function prototypes
void init_ethernet(void);
void start_webserver(void);
void init_esp_now(void);
void init_wifi_ap_mode(void);
void init_wifi_sta_mode(const char *ssid, const char *password);

static const char *TAG = "home_base";
static esp_netif_t *eth_netif = NULL;

// Global state for initialization
static volatile bool g_ethernet_connected = false;
static volatile bool g_wifi_connected = false;

// Event handler for Ethernet
static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    switch (event_id) {
        case ETHERNET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Ethernet Link Up");
            g_ethernet_connected = true;
            break;
        case ETHERNET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Ethernet Link Down");
            g_ethernet_connected = false;
            break;
        case ETHERNET_EVENT_START:
            ESP_LOGI(TAG, "Ethernet Started");
            break;
        case ETHERNET_EVENT_STOP:
            ESP_LOGI(TAG, "Ethernet Stopped");
            break;
        default:
            break;
    }
}

// Event handler for IP
static void ip_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_ETH_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Ethernet Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        g_ethernet_connected = true;
    }
}

void app_main(void)
{
    // 1. Initialize NVS and device config
    ESP_ERROR_CHECK(device_config_init());
    ESP_ERROR_CHECK(device_config_load());

    ESP_LOGI(TAG, "=== Home Base Firmware Start ===");
    
    // 2. Initialize networking infrastructure
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(ETHERNET_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &ip_event_handler, NULL));

    // 3. Initialize Ethernet (primary interface)
    init_ethernet();

    // 4. Initialize log storage
    log_storage_init();

    // 5. Initialize ESP-NOW mesh
    init_esp_now();

    // 6. Start HTTP Server (serves both config portal and API endpoints)
    start_webserver();

    // 6. Check if device is configured
    const device_config_t *config = device_config_get();
    if (!device_config_is_configured()) {
        ESP_LOGW(TAG, "Device not configured. Starting AP mode for setup...");
        // AP mode will be started in http_server.c for serving config portal
    } else {
        ESP_LOGI(TAG, "Device configured as: %s (network_id=%d)",
                 config->device_id, config->network_id);
    }

    ESP_LOGI(TAG, "Home Base Initialized Successfully");
    ESP_LOGI(TAG, "=== Ready for ESP-NOW Mesh Messages ===");

    // Main loop
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        if (g_ethernet_connected) {
            ESP_LOGD(TAG, "Ethernet connected, ready to forward logs to Unraid");
        }
    }
}

// Ethernet initialization for P4-ETH-M with IP101 PHY and POE support
void init_ethernet(void) {
    ESP_LOGI(TAG, "Initializing Ethernet (P4-ETH-M with IP101 PHY)...");

    // Check for POE power input (GPIO38 detects power)
    // In real implementation, monitor GPIO38 ADC or digital input
    // For now, log POE capability
    ESP_LOGI(TAG, "POE power input available on GPIO38");

    // Create new default instance of esp-netif for Ethernet
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    eth_netif = esp_netif_new(&netif_cfg);

    // Init MAC and PHY configs to default
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    // P4-ETH-M specific PHY configuration
    // IP101 PHY at address 1 on the RMII interface
    phy_config.phy_addr = 1;
    phy_config.reset_gpio_num = -1;  // No manual reset needed, managed internally

    // ESP32-P4 specific EMAC configuration
    // P4 has internal MAC, configure SMI (Management Interface) GPIO pins
    // P4-ETH-M uses default MDC/MDIO pins: MDC on GPIO21, MDIO on GPIO22
    eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    esp32_emac_config.smi_mdc_gpio_num = 21;    // MDC (Management Data Clock)
    esp32_emac_config.smi_mdio_gpio_num = 22;   // MDIO (Management Data I/O)

    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config);
    if (!mac) {
        ESP_LOGE(TAG, "Failed to create Ethernet MAC");
        return;
    }

    esp_eth_phy_t *phy = esp_eth_phy_new_ip101(&phy_config);
    if (!phy) {
        ESP_LOGE(TAG, "Failed to create Ethernet PHY");
        return;
    }

    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;
    
    ESP_ERROR_CHECK(esp_eth_driver_install(&config, &eth_handle));
    if (!eth_handle) {
        ESP_LOGE(TAG, "Failed to install Ethernet driver");
        return;
    }

    // Attach Ethernet driver to TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));

    // Start Ethernet
    ESP_ERROR_CHECK(esp_eth_start(eth_handle));
    
    ESP_LOGI(TAG, "Ethernet (P4-ETH-M) initialized successfully");
    ESP_LOGI(TAG, "Waiting for Ethernet link...");
}
