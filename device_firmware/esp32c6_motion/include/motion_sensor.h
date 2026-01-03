#ifndef MOTION_SENSOR_H
#define MOTION_SENSOR_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/**
 * Motion event structure
 */
typedef struct {
    uint32_t timestamp;     // When motion was detected
    bool motion_detected;   // True if motion, false if cleared
    uint8_t gpio;          // Which GPIO triggered
} motion_event_t;

/**
 * Callback function type for motion events
 */
typedef void (*motion_event_callback_t)(const motion_event_t *event);

/**
 * Initialize motion sensor
 * @param callback Function to call when motion detected (can be NULL)
 */
esp_err_t motion_sensor_init(motion_event_callback_t callback);

/**
 * Start task that processes motion events from queue
 * Call this after init to begin event handling
 */
void motion_sensor_start_task(void);

/**
 * Motion sensor task (called automatically by start_task)
 * Processes motion events from interrupt queue
 */
void motion_sensor_task(void *pvParameters);

/**
 * Check if motion is currently detected
 */
bool motion_sensor_is_motion_detected(void);

/**
 * Get milliseconds since last motion detection
 */
uint32_t motion_sensor_time_since_motion(void);

/**
 * Update motion sensor sensitivity (1-10 scale)
 * Sensitivity affects detection threshold - higher = more sensitive
 * NOTE: Actual threshold implementation depends on ADC or GPIO level
 */
void motion_sensor_set_sensitivity(uint8_t sensitivity);

/**
 * Update motion cooldown period (5000-300000ms)
 * Prevents rapid re-triggering of motion events
 */
void motion_sensor_set_cooldown(uint32_t cooldown_ms);

/**
 * Deinit motion sensor and cleanup
 */
void motion_sensor_deinit(void);

#endif // MOTION_SENSOR_H
