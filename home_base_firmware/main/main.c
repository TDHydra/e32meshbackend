#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "nvs_flash.h"
#include "protocol.h"

// Function prototypes
void init_ethernet(void);
void start_webserver(void);
void init_esp_now(void);

static const char *TAG = "home_base";

void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 1. Initialize Ethernet (IP101)
    init_ethernet();

    // 2. Initialize ESP-NOW
    init_esp_now();

    // 3. Start HTTP Server (will wait for IP)
    start_webserver();

    ESP_LOGI(TAG, "Home Base Initialized");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Minimal Ethernet Init (IP101)
void init_ethernet(void) {
    ESP_LOGI(TAG, "Initializing Ethernet (IP101)...");

    // Create new default instance of esp-netif for Ethernet
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&netif_cfg);

    // Init MAC and PHY configs to default
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    // Update PHY config
    phy_config.phy_addr = 1;
    phy_config.reset_gpio_num = -1; // Set if known

    // ESP-IDF 5.x specific config for internal EMAC
    eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();

    // Note: You must configure the SMI/MDC GPIOs here depending on the board
    // esp32_emac_config.smi_mdc_gpio_num = 23;
    // esp32_emac_config.smi_mdio_gpio_num = 18;

    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_ip101(&phy_config);

    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;
    ESP_ERROR_CHECK(esp_eth_driver_install(&config, &eth_handle));

    // Attach Ethernet driver to TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));

    ESP_ERROR_CHECK(esp_eth_start(eth_handle));
}
