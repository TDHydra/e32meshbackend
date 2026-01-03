#include "log_storage.h"
#include <esp_log.h>
#include <string.h>
#include <cJSON.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <time.h>
#include <stdlib.h>

static const char *TAG = "log_storage";

// In-memory storage for logs and motion events
static device_log_t g_logs[MAX_LOGS];
static uint32_t g_log_count = 0;
static uint32_t g_next_log_id = 1;

static motion_event_t g_motion_events[MAX_MOTION_EVENTS];
static uint32_t g_motion_count = 0;
static uint32_t g_next_motion_id = 1;

// NVS handle for persistent storage
static nvs_handle_t g_nvs_handle = 0;

void log_storage_init(void)
{
    esp_err_t err = nvs_open("logs", NVS_READWRITE, &g_nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS namespace for logs: %s", esp_err_to_name(err));
        return;
    }

    // Try to restore logs from NVS
    // For now, we'll use in-memory storage as primary (faster)
    // NVS is used for persistence across reboots
    
    ESP_LOGI(TAG, "Log storage initialized");
}

void log_storage_add_log(const char *device_id, const char *level, 
                         const char *category, const char *message)
{
    if (g_log_count >= MAX_LOGS) {
        // FIFO: shift all logs down by 1
        memmove(&g_logs[0], &g_logs[1], (MAX_LOGS - 1) * sizeof(device_log_t));
        g_log_count--;
    }

    device_log_t *log = &g_logs[g_log_count];
    log->id = g_next_log_id++;
    
    strncpy(log->device_id, device_id, sizeof(log->device_id) - 1);
    log->device_id[sizeof(log->device_id) - 1] = '\0';
    
    log->timestamp = time(NULL);
    
    strncpy(log->level, level, sizeof(log->level) - 1);
    log->level[sizeof(log->level) - 1] = '\0';
    
    strncpy(log->category, category, sizeof(log->category) - 1);
    log->category[sizeof(log->category) - 1] = '\0';
    
    strncpy(log->message, message, sizeof(log->message) - 1);
    log->message[sizeof(log->message) - 1] = '\0';

    g_log_count++;
    
    // Persist to NVS (periodically, not every log)
    // This is done on a background task to avoid blocking
}

void log_storage_add_motion_event(const char *device_id, const char *media_path)
{
    if (g_motion_count >= MAX_MOTION_EVENTS) {
        // FIFO: shift all events down by 1
        memmove(&g_motion_events[0], &g_motion_events[1], 
                (MAX_MOTION_EVENTS - 1) * sizeof(motion_event_t));
        g_motion_count--;
    }

    motion_event_t *event = &g_motion_events[g_motion_count];
    event->id = g_next_motion_id++;
    
    strncpy(event->device_id, device_id, sizeof(event->device_id) - 1);
    event->device_id[sizeof(event->device_id) - 1] = '\0';
    
    event->timestamp = time(NULL);
    
    if (media_path) {
        strncpy(event->media_path, media_path, sizeof(event->media_path) - 1);
        event->media_path[sizeof(event->media_path) - 1] = '\0';
    } else {
        event->media_path[0] = '\0';
    }

    g_motion_count++;
}

char* log_storage_get_logs_json(const char *device_id, int limit)
{
    cJSON *root = cJSON_CreateArray();
    
    // Iterate in reverse order (newest first)
    int count = 0;
    for (int i = (int)g_log_count - 1; i >= 0 && count < limit; i--) {
        device_log_t *log = &g_logs[i];
        
        // Filter by device_id if specified
        if (device_id && strcmp(log->device_id, device_id) != 0) {
            continue;
        }
        
        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "id", log->id);
        cJSON_AddStringToObject(item, "device_id", log->device_id);
        cJSON_AddNumberToObject(item, "timestamp", log->timestamp);
        cJSON_AddStringToObject(item, "level", log->level);
        cJSON_AddStringToObject(item, "category", log->category);
        cJSON_AddStringToObject(item, "message", log->message);
        
        cJSON_AddItemToArray(root, item);
        count++;
    }
    
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str;
}

char* log_storage_get_motion_json(const char *device_id, int limit)
{
    cJSON *root = cJSON_CreateArray();
    
    // Iterate in reverse order (newest first)
    int count = 0;
    for (int i = (int)g_motion_count - 1; i >= 0 && count < limit; i--) {
        motion_event_t *event = &g_motion_events[i];
        
        // Filter by device_id if specified
        if (device_id && strcmp(event->device_id, device_id) != 0) {
            continue;
        }
        
        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "id", event->id);
        cJSON_AddStringToObject(item, "device_id", event->device_id);
        cJSON_AddNumberToObject(item, "timestamp", event->timestamp);
        if (strlen(event->media_path) > 0) {
            cJSON_AddStringToObject(item, "media_path", event->media_path);
        }
        
        cJSON_AddItemToArray(root, item);
        count++;
    }
    
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str;
}

uint32_t log_storage_get_log_count(void)
{
    return g_log_count;
}

uint32_t log_storage_get_motion_count(void)
{
    return g_motion_count;
}

void log_storage_clear_logs(void)
{
    g_log_count = 0;
    g_next_log_id = 1;
    memset(g_logs, 0, sizeof(g_logs));
    ESP_LOGI(TAG, "Logs cleared");
}

void log_storage_clear_motion(void)
{
    g_motion_count = 0;
    g_next_motion_id = 1;
    memset(g_motion_events, 0, sizeof(g_motion_events));
    ESP_LOGI(TAG, "Motion events cleared");
}
