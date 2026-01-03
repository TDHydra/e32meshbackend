#ifndef DISPLAY_UI_H
#define DISPLAY_UI_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Initialize display UI layer
 */
void display_ui_init(void);

/**
 * Show motion detection status
 * Updates display with current motion state and time since last detection
 * 
 * @param motion_detected True if motion currently detected
 * @param time_since_motion_ms Milliseconds since last motion event
 */
void display_ui_show_motion_status(bool motion_detected, uint32_t time_since_motion_ms);

/**
 * Show splash screen with message
 * @param message Text to display
 */
void display_ui_show_splash(const char *message);

/**
 * Show network and device information
 * @param network_id Network ID the device belongs to
 * @param device_id Device identifier string
 */
void display_ui_show_network_info(int network_id, const char *device_id);

/**
 * Show error message
 * @param error_message Error text to display
 */
void display_ui_show_error(const char *error_message);

/**
 * Show connecting/status message
 * @param status Status message (e.g., "Connecting to WiFi...")
 */
void display_ui_show_connecting(const char *status);

/**
 * Update display brightness
 * @param brightness 0-100 (percentage)
 */
void display_ui_update_brightness(uint8_t brightness);

#endif // DISPLAY_UI_H
