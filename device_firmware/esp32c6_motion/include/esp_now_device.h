#ifndef ESP_NOW_DEVICE_H
#define ESP_NOW_DEVICE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// Forward declare mesh_message_t
typedef struct {
    uint8_t type;
    char device_id[16];
    uint32_t timestamp;
    char payload[200];
    uint8_t signature[64];
} mesh_message_t;

/**
 * Handler function type for received ESP-NOW messages
 */
typedef void (*esp_now_message_handler_t)(const mesh_message_t *msg);

/**
 * Initialize ESP-NOW in device mode (STA)
 * Prepares device to send/receive messages from home base
 */
esp_err_t esp_now_device_init(void);

/**
 * Add home base as an ESP-NOW peer
 * @param peer_mac MAC address of home base (6 bytes)
 */
esp_err_t esp_now_device_add_peer(const uint8_t *peer_mac);

/**
 * Send motion event to home base
 * @param peer_mac Home base MAC address
 * @param timestamp Event timestamp (Unix seconds)
 * @param motion_detected True if motion detected, false if cleared
 * @param signature Ed25519 signature of the motion event (64 bytes)
 */
esp_err_t esp_now_device_send_motion_event(const uint8_t *peer_mac,
                                            uint32_t timestamp,
                                            bool motion_detected,
                                            const uint8_t *signature);

/**
 * Send heartbeat to home base
 * @param peer_mac Home base MAC address
 * @param timestamp Current timestamp
 */
esp_err_t esp_now_device_send_heartbeat(const uint8_t *peer_mac, uint32_t timestamp);

/**
 * Process all queued ESP-NOW messages
 * @param handler Optional callback for message processing
 */
void esp_now_device_process_messages(esp_now_message_handler_t handler);

/**
 * Deinit ESP-NOW and cleanup
 */
void esp_now_device_deinit(void);

#endif // ESP_NOW_DEVICE_H
