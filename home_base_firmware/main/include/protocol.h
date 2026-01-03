#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

// ESP-NOW Message Types
#define MSG_TYPE_HEARTBEAT 0x01
#define MSG_TYPE_MOTION    0x02
#define MSG_TYPE_LOG       0x03
#define MSG_TYPE_COMMAND   0x04

// Structure matching the requirement
typedef struct __attribute__((packed)) {
    uint8_t type;            // 1 byte
    char device_id[16];      // 16 bytes, null-terminated
    uint32_t timestamp;      // 4 bytes
    char payload[200];       // 200 bytes, JSON data
    uint8_t signature[64];   // 64 bytes, Ed25519 signature
} mesh_message_t;

#endif // PROTOCOL_H
