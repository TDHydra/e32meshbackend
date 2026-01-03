#ifndef LOG_STORAGE_H
#define LOG_STORAGE_H

#include <stdint.h>
#include <time.h>

#define MAX_LOGS 500
#define MAX_MOTION_EVENTS 100

/**
 * Device log entry
 */
typedef struct {
    uint32_t id;
    char device_id[32];
    uint64_t timestamp;  // Unix timestamp in seconds
    char level[16];      // "info", "warning", "error"
    char category[32];   // "sensor", "network", "system"
    char message[256];   // Log message
} device_log_t;

/**
 * Motion event entry
 */
typedef struct {
    uint32_t id;
    char device_id[32];
    uint64_t timestamp;  // Unix timestamp in seconds
    char media_path[128]; // Path to captured image/video
} motion_event_t;

/**
 * Initialize log storage (NVS)
 */
void log_storage_init(void);

/**
 * Add a log entry (FIFO - oldest entries removed when full)
 */
void log_storage_add_log(const char *device_id, const char *level, 
                         const char *category, const char *message);

/**
 * Add a motion event
 */
void log_storage_add_motion_event(const char *device_id, const char *media_path);

/**
 * Get all logs (optional device_id filter)
 * Returns JSON array string (caller must free)
 */
char* log_storage_get_logs_json(const char *device_id, int limit);

/**
 * Get all motion events (optional device_id filter)
 * Returns JSON array string (caller must free)
 */
char* log_storage_get_motion_json(const char *device_id, int limit);

/**
 * Get count of stored logs
 */
uint32_t log_storage_get_log_count(void);

/**
 * Get count of stored motion events
 */
uint32_t log_storage_get_motion_count(void);

/**
 * Clear all logs (for debugging)
 */
void log_storage_clear_logs(void);

/**
 * Clear all motion events (for debugging)
 */
void log_storage_clear_motion(void);

#endif // LOG_STORAGE_H
