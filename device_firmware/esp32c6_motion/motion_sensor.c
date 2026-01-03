#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "device_config.h"
#include "motion_sensor.h"

static const char *TAG = "motion";

// Motion event queue
static QueueHandle_t motion_queue = NULL;
static motion_event_callback_t motion_callback = NULL;

// Debounce/cooldown tracking
static uint32_t last_motion_time = 0;
static bool motion_detected = false;

/**
 * GPIO interrupt handler for motion sensor
 * Triggered when motion is detected
 */
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t now = xTaskGetTickCountFromISR();
    uint32_t since_last = (now - last_motion_time) / portTICK_PERIOD_MS;
    
    const device_config_t *config = (const device_config_t *)arg;
    
    // Only trigger if outside cooldown window
    if (since_last > config->motion_cooldown_ms) {
        motion_event_t event = {
            .timestamp = now,
            .motion_detected = true,
            .gpio = config->motion_gpio
        };
        
        // Send to queue (non-blocking)
        xQueueSendFromISR(motion_queue, &event, NULL);
        
        last_motion_time = now;
    }
}

esp_err_t motion_sensor_init(motion_event_callback_t callback)
{
    const device_config_t *config = device_config_get();
    
    ESP_LOGI(TAG, "Initializing motion sensor on GPIO %d", config->motion_gpio);
    
    // Create event queue
    motion_queue = xQueueCreate(10, sizeof(motion_event_t));
    if (!motion_queue) {
        ESP_LOGE(TAG, "Failed to create motion event queue");
        return ESP_ERR_NO_MEM;
    }
    
    motion_callback = callback;
    
    // Configure GPIO as input with interrupt
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_POSEDGE,  // Trigger on rising edge
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << config->motion_gpio),
        .pull_down_en = 0,
        .pull_up_en = 1,  // Internal pull-up
    };
    
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    
    // Install ISR handler
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(config->motion_gpio, gpio_isr_handler, (void *)config));
    
    ESP_LOGI(TAG, "Motion sensor initialized (sensitivity=%d, cooldown=%dms)",
             config->motion_sensitivity, config->motion_cooldown_ms);
    
    return ESP_OK;
}

void motion_sensor_start_task(void)
{
    // Create task to handle motion events from queue
    xTaskCreate(motion_sensor_task, "motion_task", 4096, NULL, 5, NULL);
}

void motion_sensor_task(void *pvParameters)
{
    motion_event_t event;
    
    while (1) {
        if (xQueueReceive(motion_queue, &event, pdMS_TO_TICKS(1000))) {
            ESP_LOGI(TAG, "Motion detected! (GPIO %d)", event.gpio);
            
            motion_detected = true;
            
            // Call user callback if registered
            if (motion_callback) {
                motion_callback(&event);
            }
            
            // Cooldown handling could be added here
            vTaskDelay(pdMS_TO_TICKS(100));
            motion_detected = false;
        }
    }
}

bool motion_sensor_is_motion_detected(void)
{
    return motion_detected;
}

uint32_t motion_sensor_time_since_motion(void)
{
    return (xTaskGetTickCount() - last_motion_time) / portTICK_PERIOD_MS;
}

void motion_sensor_set_sensitivity(uint8_t sensitivity)
{
    if (sensitivity < 1) sensitivity = 1;
    if (sensitivity > 10) sensitivity = 10;
    
    device_config_t config = *device_config_get();
    config.motion_sensitivity = sensitivity;
    device_config_save(&config);
    
    ESP_LOGI(TAG, "Motion sensitivity updated to %d", sensitivity);
}

void motion_sensor_set_cooldown(uint32_t cooldown_ms)
{
    if (cooldown_ms < 5000) cooldown_ms = 5000;      // Minimum 5 seconds
    if (cooldown_ms > 300000) cooldown_ms = 300000;  // Maximum 5 minutes
    
    device_config_t config = *device_config_get();
    config.motion_cooldown_ms = cooldown_ms;
    device_config_save(&config);
    
    ESP_LOGI(TAG, "Motion cooldown updated to %ldms", cooldown_ms);
}

void motion_sensor_deinit(void)
{
    const device_config_t *config = device_config_get();
    gpio_isr_handler_remove(config->motion_gpio);
    
    if (motion_queue) {
        vQueueDelete(motion_queue);
        motion_queue = NULL;
    }
}
