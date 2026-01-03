#include <string.h>
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "device_config.h"
#include "display_driver.h"

static const char *TAG = "display";

static spi_device_handle_t spi_handle = NULL;
static const device_config_t *config = NULL;

// ST7789 command codes
#define ST7789_NOP        0x00
#define ST7789_SWRESET    0x01
#define ST7789_RDDID      0x04
#define ST7789_RDDST      0x09
#define ST7789_SLPIN      0x10
#define ST7789_SLPOUT     0x11
#define ST7789_PTLON      0x12
#define ST7789_NORON      0x13
#define ST7789_INVOFF     0x20
#define ST7789_INVON      0x21
#define ST7789_DISPOFF    0x28
#define ST7789_DISPON     0x29
#define ST7789_CASET      0x2A
#define ST7789_RASET      0x2B
#define ST7789_RAMWR      0x2C
#define ST7789_RAMRD      0x2E
#define ST7789_COLMOD     0x3A
#define ST7789_MADCTL     0x36
#define ST7789_VSCSAD     0x37

// Helper to send SPI command
static void write_command(uint8_t cmd)
{
    // DC low for command
    gpio_set_level(config->display_dc_gpio, 0);
    vTaskDelay(pdMS_TO_TICKS(1));
    
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    spi_device_polling_transmit(spi_handle, &t);
    
    gpio_set_level(config->display_dc_gpio, 1);
    vTaskDelay(pdMS_TO_TICKS(1));
}

// Helper to send SPI data
static void write_data(const uint8_t *data, size_t len)
{
    // DC high for data
    gpio_set_level(config->display_dc_gpio, 1);
    vTaskDelay(pdMS_TO_TICKS(1));
    
    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = data,
    };
    spi_device_polling_transmit(spi_handle, &t);
}

// Helper to send 8-bit data
static void write_data_byte(uint8_t data)
{
    write_data(&data, 1);
}

// Helper to send 16-bit data (big-endian)
static void write_data_word(uint16_t data)
{
    uint8_t buf[2] = {
        (data >> 8) & 0xFF,
        data & 0xFF
    };
    write_data(buf, 2);
}

esp_err_t display_init(void)
{
    config = device_config_get();
    if (!config->display_enabled) {
        ESP_LOGW(TAG, "Display is disabled in config");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing ST7789 display...");
    
    // Initialize GPIO pins
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << config->display_dc_gpio) |
                        (1ULL << config->display_reset_gpio) |
                        (1ULL << config->display_backlight_gpio),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    // Initialize SPI
    spi_bus_config_t buscfg = {
        .miso_io_num = config->display_miso_gpio,
        .mosi_io_num = config->display_mosi_gpio,
        .sclk_io_num = config->display_sclk_gpio,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t devcfg = {
        .mode = 0,
        .clock_speed_hz = 40 * 1000 * 1000,
        .spics_io_num = config->display_cs_gpio,
        .queue_size = 7,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI3_HOST, &devcfg, &spi_handle));

    // Reset display
    gpio_set_level(config->display_reset_gpio, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(config->display_reset_gpio, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(config->display_reset_gpio, 1);
    vTaskDelay(pdMS_TO_TICKS(120));

    // Initialize ST7789
    write_command(ST7789_SWRESET);      // Software reset
    vTaskDelay(pdMS_TO_TICKS(150));

    write_command(ST7789_SLPOUT);       // Sleep out
    vTaskDelay(pdMS_TO_TICKS(50));

    write_command(ST7789_COLMOD);       // Color mode: 16-bit/pixel
    write_data_byte(0x05);              // 16-bit color

    write_command(ST7789_MADCTL);       // Memory access control
    write_data_byte(0x00);              // Default orientation

    write_command(ST7789_INVON);        // Inversion ON
    vTaskDelay(pdMS_TO_TICKS(10));

    write_command(ST7789_NORON);        // Normal display mode
    vTaskDelay(pdMS_TO_TICKS(10));

    write_command(ST7789_DISPON);       // Display ON
    vTaskDelay(pdMS_TO_TICKS(50));

    // Set backlight brightness
    gpio_set_level(config->display_backlight_gpio, 1);
    
    ESP_LOGI(TAG, "ST7789 display initialized (240x320)");
    return ESP_OK;
}

void display_fill_color(uint16_t color)
{
    if (!config || !config->display_enabled || !spi_handle) {
        return;
    }

    // Set column address
    write_command(ST7789_CASET);
    write_data_word(0);                    // X start
    write_data_word(config->display_width - 1);  // X end

    // Set row address
    write_command(ST7789_RASET);
    write_data_word(0);                    // Y start
    write_data_word(config->display_height - 1); // Y end

    // Write RAM
    write_command(ST7789_RAMWR);

    // Fill with color
    uint16_t pixel_count = config->display_width * config->display_height;
    uint8_t color_hi = (color >> 8) & 0xFF;
    uint8_t color_lo = color & 0xFF;

    // Send in chunks to avoid stack overflow
    uint8_t buf[512];
    for (int i = 0; i < sizeof(buf); i += 2) {
        buf[i] = color_hi;
        buf[i + 1] = color_lo;
    }

    for (uint16_t i = 0; i < pixel_count; i += (sizeof(buf) / 2)) {
        uint16_t count = (pixel_count - i > sizeof(buf) / 2) ? sizeof(buf) : (pixel_count - i) * 2;
        write_data(buf, count);
    }
}

void display_set_window(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    if (!config || !config->display_enabled || !spi_handle) {
        return;
    }

    // Set column address
    write_command(ST7789_CASET);
    write_data_word(x);
    write_data_word(x + w - 1);

    // Set row address
    write_command(ST7789_RASET);
    write_data_word(y);
    write_data_word(y + h - 1);
}

void display_draw_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (!config || !config->display_enabled || !spi_handle) {
        return;
    }

    display_set_window(x, y, 1, 1);
    write_command(ST7789_RAMWR);
    write_data_word(color);
}

void display_set_brightness(uint8_t brightness)
{
    if (!config || !config->display_enabled) {
        return;
    }

    if (brightness > 100) brightness = 100;
    
    // PWM would be ideal, but for simplicity we use on/off with backlight GPIO
    if (brightness > 50) {
        gpio_set_level(config->display_backlight_gpio, 1);
    } else {
        gpio_set_level(config->display_backlight_gpio, 0);
    }
}

void display_deinit(void)
{
    if (spi_handle) {
        spi_bus_remove_device(spi_handle);
        spi_bus_free(SPI3_HOST);
        spi_handle = NULL;
    }
}
