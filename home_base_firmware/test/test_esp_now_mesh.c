/*
 * Test for ESP-NOW mesh message reception and parsing
 * 
 * Pattern: Based on ESP-IDF esp_now examples
 * Validates that OnDataRecv correctly parses mesh_message_t structures
 * and forwards logs to backend API
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "unity.h"
#include "esp_log.h"
#include "esp_now.h"
#include "protocol.h"

static const char *TAG = "test_esp_now_mesh";

// Mock for pending log queue
static int pending_log_count = 0;
static mesh_message_t pending_logs[10];

// Forward declaration (from esp_now_mesh.c)
extern void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len);

// Mock implementation of send_log_to_unraid
void send_log_to_unraid(mesh_message_t *msg) {
    if (pending_log_count < 10) {
        memcpy(&pending_logs[pending_log_count], msg, sizeof(mesh_message_t));
        pending_log_count++;
    }
}

TEST_CASE("OnDataRecv rejects wrong message size", "[esp_now]") {
    uint8_t bad_data[100] = {0};  // Wrong size
    uint8_t mac_addr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    
    pending_log_count = 0;
    OnDataRecv(mac_addr, bad_data, 100);
    
    // Message should be rejected, no logs queued
    TEST_ASSERT_EQUAL(0, pending_log_count);
}

TEST_CASE("OnDataRecv accepts valid MSG_TYPE_LOG", "[esp_now]") {
    mesh_message_t msg = {
        .type = MSG_TYPE_LOG,
        .device_id = "ESP32-ABC123\0\0\0\0",
        .timestamp = 1704268800,
        .payload = "{\"level\":\"INFO\",\"message\":\"test log\"}",
        .signature = {0}  // Empty signature for test
    };
    
    uint8_t mac_addr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    
    pending_log_count = 0;
    OnDataRecv(mac_addr, (uint8_t*)&msg, sizeof(mesh_message_t));
    
    // Log should be queued for sending to Unraid
    TEST_ASSERT_EQUAL(1, pending_log_count);
    TEST_ASSERT_EQUAL_STRING("ESP32-ABC123", pending_logs[0].device_id);
}

TEST_CASE("OnDataRecv accepts valid MSG_TYPE_MOTION", "[esp_now]") {
    mesh_message_t msg = {
        .type = MSG_TYPE_MOTION,
        .device_id = "ESP32-DEF456\0\0\0\0",
        .timestamp = 1704268801,
        .payload = "{\"area\":\"living_room\",\"confidence\":95}",
        .signature = {0}
    };
    
    uint8_t mac_addr[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    
    pending_log_count = 0;
    OnDataRecv(mac_addr, (uint8_t*)&msg, sizeof(mesh_message_t));
    
    // Motion events should be queued
    TEST_ASSERT_EQUAL(1, pending_log_count);
    TEST_ASSERT_EQUAL(MSG_TYPE_MOTION, pending_logs[0].type);
}

TEST_CASE("OnDataRecv ignores MSG_TYPE_HEARTBEAT", "[esp_now]") {
    mesh_message_t msg = {
        .type = MSG_TYPE_HEARTBEAT,
        .device_id = "ESP32-GHI789\0\0\0\0",
        .timestamp = 1704268802,
        .payload = "{\"rssi\":-45,\"heap\":120000}",
        .signature = {0}
    };
    
    uint8_t mac_addr[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    
    pending_log_count = 0;
    OnDataRecv(mac_addr, (uint8_t*)&msg, sizeof(mesh_message_t));
    
    // Heartbeats are not forwarded to Unraid (handled locally)
    TEST_ASSERT_EQUAL(0, pending_log_count);
}

TEST_CASE("OnDataRecv handles batch of mixed message types", "[esp_now]") {
    uint8_t mac_addr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    
    pending_log_count = 0;
    
    // Send LOG message
    mesh_message_t log_msg = {
        .type = MSG_TYPE_LOG,
        .device_id = "ESP32-001\0\0\0\0\0\0\0",
        .timestamp = 1704268800,
        .payload = "{\"message\":\"test\"}",
        .signature = {0}
    };
    OnDataRecv(mac_addr, (uint8_t*)&log_msg, sizeof(mesh_message_t));
    
    // Send HEARTBEAT (should be ignored)
    mesh_message_t hb_msg = {
        .type = MSG_TYPE_HEARTBEAT,
        .device_id = "ESP32-001\0\0\0\0\0\0\0",
        .timestamp = 1704268801,
        .payload = "{\"rssi\":-50}",
        .signature = {0}
    };
    OnDataRecv(mac_addr, (uint8_t*)&hb_msg, sizeof(mesh_message_t));
    
    // Send MOTION message
    mesh_message_t motion_msg = {
        .type = MSG_TYPE_MOTION,
        .device_id = "ESP32-001\0\0\0\0\0\0\0",
        .timestamp = 1704268802,
        .payload = "{\"area\":\"kitchen\"}",
        .signature = {0}
    };
    OnDataRecv(mac_addr, (uint8_t*)&motion_msg, sizeof(mesh_message_t));
    
    // Only LOG and MOTION should be queued
    TEST_ASSERT_EQUAL(2, pending_log_count);
    TEST_ASSERT_EQUAL(MSG_TYPE_LOG, pending_logs[0].type);
    TEST_ASSERT_EQUAL(MSG_TYPE_MOTION, pending_logs[1].type);
}

TEST_CASE("OnDataRecv preserves device_id and timestamp", "[esp_now]") {
    mesh_message_t msg = {
        .type = MSG_TYPE_LOG,
        .device_id = "ESP32-TEST123\0\0\0",
        .timestamp = 1234567890,
        .payload = "{\"data\":\"test\"}",
        .signature = {0}
    };
    
    uint8_t mac_addr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    
    pending_log_count = 0;
    OnDataRecv(mac_addr, (uint8_t*)&msg, sizeof(mesh_message_t));
    
    TEST_ASSERT_EQUAL(1, pending_log_count);
    TEST_ASSERT_EQUAL_STRING("ESP32-TEST123", pending_logs[0].device_id);
    TEST_ASSERT_EQUAL(1234567890, pending_logs[0].timestamp);
}
