# Quick Reference - Phase 2 Implementation

## What Was Built This Session

### Firmware Endpoints (Home Base)
```
✅ GET  /api/logs              Device logs with filtering
✅ GET  /api/motion            Motion events with filtering
✅ POST /api/v1/command        Send commands to devices
```

### Device Firmware (ESP32-S3)
```
✅ WiFi modes (AP for config, STA for normal operation)
✅ NVS configuration storage (30+ parameters)
✅ ESP-NOW mesh initialization
✅ Message builders (heartbeat, motion, log)
✅ Configuration portal ready for user setup
```

### Testing
```
✅ 21-test integration test suite
✅ Coverage: API endpoints, device config, message format, error handling, performance
✅ Manual testing examples with curl
✅ Debugging guide for common issues
```

---

## Quick Start

### Build Home Base
```bash
cd home_base_firmware
idf.py set-target esp32p4
idf.py build
idf.py -p COM3 flash monitor
```

Expected: "Web server started with 15 endpoints" + "Ethernet Link Up"

### Build Device
```bash
cd device_firmware
idf.py set-target esp32s3
idf.py build
idf.py -p COM4 flash monitor
```

Expected: "ESP32-MESH-XXXXXX" WiFi network appears

### Test APIs
```bash
# Status
curl http://192.168.1.100/api/v1/status | jq

# Logs
curl "http://192.168.1.100/api/logs?limit=10" | jq

# Motion events
curl "http://192.168.1.100/api/motion" | jq

# Send command
curl -X POST http://192.168.1.100/api/v1/command \
  -H "Content-Type: application/json" \
  -d '{"command":"test","target_device":"device-1"}'
```

### Run Tests
```bash
pytest tests/integration_tests.py -v
```

---

## File Locations

| Component | File | Lines | Type |
|-----------|------|-------|------|
| Log Storage Interface | `home_base_firmware/main/include/log_storage.h` | 65 | Header |
| Log Storage Impl | `home_base_firmware/main/log_storage.c` | 260 | Source |
| New Endpoints | `home_base_firmware/main/http_server.c` | +150 | Modified |
| Device Firmware API | `device_firmware/components/device_firmware/include/device_firmware.h` | 85 | Header |
| Device Firmware Impl | `device_firmware/components/device_firmware/device_firmware.c` | 350 | Source |
| Test Suite | `tests/integration_tests.py` | 420 | Tests |
| Testing Guide | `INTEGRATION_TESTING.md` | 350+ | Doc |
| Completion Summary | `PHASE2_COMPLETION.md` | 500+ | Doc |

---

## Endpoint Specifications

### GET /api/logs
```
Query Parameters:
  - device_id (optional): Filter by device
  - limit (optional): Max results (default 100, max 1000)

Response:
  200 OK
  [
    {
      "id": 1,
      "device_id": "sensor-001",
      "timestamp": 1704307200,
      "level": "info",
      "category": "system",
      "message": "Device initialized"
    },
    ...
  ]

Errors:
  - 500: JSON generation failed
```

### GET /api/motion
```
Query Parameters:
  - device_id (optional): Filter by device
  - limit (optional): Max results (default 100, max 1000)

Response:
  200 OK
  [
    {
      "id": 1,
      "device_id": "sensor-001",
      "timestamp": 1704307200,
      "media_path": "/sd/motion/001.jpg"
    },
    ...
  ]

Errors:
  - 500: JSON generation failed
```

### POST /api/v1/command
```
Request:
  {
    "command": "led_set_color",
    "target_device": "sensor-001",
    "payload": {...},
    "signature": "0" * 128
  }

Response:
  200 OK
  {
    "status": "queued",
    "command": "led_set_color",
    "target_device": "sensor-001"
  }

Errors:
  - 400: Invalid JSON or missing required fields
  - 413: Payload > 1KB
```

---

## Device Configuration

Device starts in AP mode if not configured:
```
AP SSID: "ESP32-MESH-AABBCC" (MAC last 3 bytes)
IP: 192.168.4.1
Password: None (open)
```

When configured, switches to STA mode using saved WiFi credentials.

Configuration storage (NVS):
```
{
  "device_id": "sensor-001",
  "network_id": 1,
  "device_type": 1,              // 1=motion, 2=camera
  "private_key": "...",           // Ed25519 hex
  "pir_gpio": 39,
  "pir_sensitivity": 5,           // 1-10
  "pir_cooldown_ms": 30000,
  "led_gpio": 48,
  "led_brightness": 80,           // 0-100
  "ssid": "MyNetwork",
  "password": "..."
}
```

---

## Test Commands

### All Tests
```bash
pytest tests/integration_tests.py -v
```

### Specific Test Class
```bash
pytest tests/integration_tests.py::TestHomeBaseAPI -v
pytest tests/integration_tests.py::TestDeviceFirmware -v
pytest tests/integration_tests.py::TestMessageFormat -v
```

### With HTML Report
```bash
pytest tests/integration_tests.py --html=report.html --self-contained-html
open report.html
```

### With Coverage
```bash
pytest tests/integration_tests.py --cov --cov-report=html
```

---

## Debugging Checklist

**Device doesn't appear in WiFi**
- [ ] Power is connected (check LED blink)
- [ ] Use serial monitor to check startup messages
- [ ] Verify device_firmware build succeeded

**API endpoint returns 404**
- [ ] Check home base serial: "15 endpoints registered"
- [ ] Verify correct IP address (192.168.1.100)
- [ ] Confirm Ethernet link is up

**Tests fail with connection refused**
- [ ] Check P4-ETH-M IP in network (ping 192.168.1.100)
- [ ] Check device IP in AP mode (connect to "ESP32-MESH-*")
- [ ] Verify ports are open (curl -v to see connection details)

**Response contains invalid JSON**
- [ ] Check http_server.c uses cJSON_Print*
- [ ] Verify no stray characters in response
- [ ] Check Content-Type header is application/json

**Performance tests timeout**
- [ ] Check home base is not overwhelmed
- [ ] Reduce system load
- [ ] Verify firmware build is optimized (-O2 flag)

---

## Next Priority Tasks

1. **WebSocket /ws Endpoint** (1-2 hours)
   - Add httpd_ws_send_frame() calls
   - Broadcast on log/motion events
   - Update dashboard to use real-time updates

2. **Message Signing** (1-2 hours)
   - Implement Ed25519 in device firmware
   - Verify in home base command handler
   - Test with integration tests

3. **Device Mesh** (2-3 hours)
   - Complete ESP-NOW send implementation
   - Add device discovery
   - Implement command routing

---

## Performance Targets

| Operation | Target | Notes |
|-----------|--------|-------|
| Status Endpoint | <50ms | Current: should be <10ms |
| Logs Endpoint | <100ms | Depends on filtering |
| Motion Endpoint | <100ms | Depends on filtering |
| Device Config | <1s | WiFi scan included |
| WebSocket Latency | <100ms | When implemented |
| Dashboard Load | <2s | With first API fetch |

---

## Architecture Notes

**Log Storage**: 
- FIFO in-memory (500 logs, 100 events)
- Oldest entries dropped when full
- Optional NVS persistence
- Total: ~189 KB RAM

**Message Format**:
- Type (1 byte) + Device ID (16) + Timestamp (4) + Payload (200) + Signature (64) = 285 bytes
- All messages signed with Ed25519 (to be implemented)
- Format for signing: `"{timestamp}:{payload}"`

**Device Firmware**:
- Configurable via web portal (AP mode)
- Persists to NVS
- Single codebase for motion + camera (compile-time selection)
- Ready for PIR sensor, RGB LED, OV2640 camera integration

---

## Known Limitations (Phase 2)

- ❌ WebSocket not yet implemented (polling fallback in dashboard)
- ❌ Message signing not yet implemented (signature placeholder)
- ❌ ESP-NOW send not complete (message builders only)
- ❌ NVS persistence not enabled (in-memory storage only)
- ❌ No PIR sensor integration yet
- ❌ No RGB LED control yet
- ❌ No camera support yet

**All limitations are documented with TODO comments in code.**

---

## References

- [Protocol Specification](home_base_firmware/main/include/protocol.h)
- [HTTP Server](home_base_firmware/main/http_server.c)
- [Device Firmware](device_firmware/components/device_firmware/device_firmware.c)
- [Integration Tests](tests/integration_tests.py)
- [Testing Guide](INTEGRATION_TESTING.md)
- [Complete Summary](PHASE2_COMPLETION.md)

---

**Last Updated**: January 3, 2026  
**Status**: ✅ Ready for Testing  
**Next Session**: Build → Flash → Test → Debug
