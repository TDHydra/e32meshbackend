# WORKSTREAM C: Device Firmware Development

## Overview
ESP-IDF firmware for motion sensors and cameras that join ESP-NOW mesh

---

## HARDWARE

| Device | Chip | Role |
|--------|------|------|
| ESP32-S3-DevKitM-1 (x2) | S3 | Motion sensor |
| Freenove ESP32-S3-WROOM CAM (x4) | S3 | Camera |
| ESP32-S3-DevKitC-1 N16R8 (x2) | S3 | Camera |
| XIAO ESP32S3 Sense (x1) | S3 | Camera (compact) |
| Waveshare P4-ETH-M (x1) | P4 | Home base (Workstream A) |

---

## FLASHING

### All ESP32-S3 Boards
```
1. Install ESP-IDF 5.2+
2. idf.py set-target esp32s3
3. Hold BOOT, press RESET, release BOOT
4. idf.py -p /dev/ttyACM0 flash monitor
```

### ESP32-P4-ETH-M (Home Base)
```
1. Use Type-C UART port (not USB OTG)
2. idf.py set-target esp32p4
3. Configure IP101 PHY in menuconfig
4. DISABLE C6 coprocessor (Ethernet only)
```

---

## FIRMWARE COMPONENTS

1. **NVS Config Storage**
   - device_id, network_id, keys, wifi creds, sensor settings

2. **WiFi + AP Mode**
   - No config → start AP "ESP32-MESH-{MAC}"
   - Config exists → connect to saved WiFi
   - 3 failures → fall back to AP

3. **ESP-NOW Mesh**
   - Join home base as peer
   - Send: heartbeat (30s), motion events, logs
   - Receive: commands from home base

4. **PIR Motion Sensor**
   - GPIO interrupt on trigger
   - Configurable sensitivity (1-10) and cooldown (5-300s)

5. **RGB LED**
   - Green=clear, Yellow=cooldown, Red=motion
   - WS2812 on GPIO48 (DevKitM) or GPIO2 (Freenove)

6. **Camera** (S3 with OV2640)
   - Capture on motion: single image, burst, or 10s video
   - Store to SD or send to home base

7. **Ed25519 Signing**
   - TinyCrypt library
   - Sign all outgoing messages
   - Verify commands from home base

8. **OTA Updates**
   - Dual partition (ota_0/ota_1)
   - Verify SHA256 + signature before applying
   - Auto-rollback on boot failure

9. **Config Web Server** (AP mode only)
   - Serve setup wizard from SPIFFS
   - Endpoints: /api/wifi/scan, /api/config/save, /api/reboot

---

## MESSAGE TYPES

| Type | Value | Direction | Purpose |
|------|-------|-----------|---------|
| HEARTBEAT | 0x01 | Device→Base | Every 30s, includes RSSI/heap/state |
| MOTION | 0x02 | Device→Base | Immediate on PIR trigger |
| LOG_BATCH | 0x03 | Device→Base | Buffered log entries |
| COMMAND | 0x04 | Base→Device | LED set, reboot, config, OTA |
| ACK | 0x05 | Device→Base | Command acknowledgment |
| PING/PONG | 0x06/07 | Both | Health check |

---

## BUILD VARIANTS

```
Motion Sensor: -DDEVICE_TYPE=motion -DHAS_CAMERA=0
Camera:        -DDEVICE_TYPE=camera -DHAS_CAMERA=1
```

---

## DELIVERABLES

1. Firmware binaries for each device variant
2. Flashing scripts with menuconfig presets
3. SPIFFS image with config portal (from Workstream B)
4. Partition table with OTA support

---

## INTEGRATION POINTS

**To Workstream A:**
- ESP-NOW message format (above)
- Device capabilities for registration

**To Workstream B:**
- Config AP endpoints
- Sensor ranges: PIR 1-10, cooldown 5-300s, brightness 0-100

---

## KEY DECISIONS MADE

1. **Ed25519** over RSA (faster on ESP32: 1-2ms vs 240-900ms)
2. **ESP-NOW** for device-to-base (low latency, 250B packets)
3. **WiFi** for base-to-server (HTTP/WebSocket)
4. **C6 disabled** on P4 home base (known SDIO stability issues)
5. **Dual OTA partitions** with auto-rollback
