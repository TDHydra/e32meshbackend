#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "display_ui.h"
#include "display_driver.h"
#include "device_config.h"

static const char *TAG = "display_ui";

/**
 * Simple 8x8 pixel font for basic text rendering
 * Minimal ASCII subset (space, 0-9, A-Z)
 */
static void draw_char(uint16_t x, uint16_t y, char c, uint16_t fg_color, uint16_t bg_color)
{
    // Very simple: just fill rectangles for demo
    // In production, use a proper font library
    
    if (c == ' ') {
        return;  // Skip spaces
    }
    
    // For demonstration, draw a simple block for each character
    // Real implementation would use a font atlas or library
    for (uint16_t py = 0; py < 8; py++) {
        for (uint16_t px = 0; px < 6; px++) {
            display_draw_pixel(x + px, y + py, fg_color);
        }
    }
}

void display_ui_init(void)
{
    ESP_LOGI(TAG, "Display UI initialized");
}

void display_ui_show_motion_status(bool motion_detected, uint32_t time_since_motion_ms)
{
    const device_config_t *config = device_config_get();
    
    if (!config->display_enabled) {
        return;
    }

    // Determine color based on motion state
    uint16_t bg_color = config->color_background;
    uint16_t status_color;
    const char *status_text;
    
    if (motion_detected) {
        status_color = config->color_motion;
        status_text = "MOTION DETECTED!";
    } else if (time_since_motion_ms < config->motion_cooldown_ms) {
        status_color = config->color_cooldown;
        status_text = "COOLDOWN...";
    } else {
        status_color = config->color_clear;
        status_text = "NO MOTION";
    }

    // Fill entire screen with status color
    display_fill_color(status_color);

    // Draw text (simplified - just show a colored rectangle and text)
    // Real implementation would draw actual text on the display
    
    // Status bar
    display_set_window(0, 0, config->display_width, 80);
    
    // Draw device ID at top
    display_set_window(10, 20, 220, 30);
    ESP_LOGI(TAG, "Motion Status: %s (time=%ldms)", status_text, time_since_motion_ms);
    
    // Draw time since last motion
    if (time_since_motion_ms > 0) {
        char time_str[32];
        uint32_t seconds = time_since_motion_ms / 1000;
        snprintf(time_str, sizeof(time_str), "%lds since motion", seconds);
        ESP_LOGI(TAG, "Time info: %s", time_str);
    }
    
    // Draw sensitivity indicator at bottom
    display_set_window(10, config->display_height - 50, 220, 40);
    char sensitivity_str[32];
    snprintf(sensitivity_str, sizeof(sensitivity_str), "Sensitivity: %d/10", 
             config->motion_sensitivity);
    ESP_LOGI(TAG, "%s", sensitivity_str);
}

void display_ui_show_splash(const char *message)
{
    const device_config_t *config = device_config_get();
    
    if (!config->display_enabled) {
        return;
    }

    // Fill with background color
    display_fill_color(config->color_background);
    
    // Draw message at center
    ESP_LOGI(TAG, "Splash: %s", message);
}

void display_ui_show_network_info(int network_id, const char *device_id)
{
    const device_config_t *config = device_config_get();
    
    if (!config->display_enabled) {
        return;
    }

    // Fill with background
    display_fill_color(config->color_background);
    
    // Draw network info
    char info[64];
    snprintf(info, sizeof(info), "Network: %d | Device: %s", network_id, device_id);
    ESP_LOGI(TAG, "Network Info: %s", info);
}

void display_ui_show_error(const char *error_message)
{
    const device_config_t *config = device_config_get();
    
    if (!config->display_enabled) {
        return;
    }

    // Fill with red background
    display_fill_color(0xF800);  // Red
    
    // Draw error message
    ESP_LOGI(TAG, "ERROR: %s", error_message);
}

void display_ui_show_connecting(const char *status)
{
    const device_config_t *config = device_config_get();
    
    if (!config->display_enabled) {
        return;
    }

    // Fill with background
    display_fill_color(config->color_background);
    
    // Draw status message
    ESP_LOGI(TAG, "Connecting: %s", status);
}

void display_ui_update_brightness(uint8_t brightness)
{
    display_set_brightness(brightness);
    ESP_LOGD(TAG, "Display brightness set to %d%%", brightness);
}
