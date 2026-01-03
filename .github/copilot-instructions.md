# E32 Mesh Backend - Copilot Instructions

## Project Overview
ESP32-based motion sensor mesh network with three-tier architecture:
1. **unraid_api/** - FastAPI backend running in Docker (Python 3.11) ✅ COMPLETE
2. **home_base_firmware/** - ESP32-P4-ETH-M coordinator firmware (ESP-IDF 5.2+, C) ✅ COMPLETE
3. **device_config_portal/** - Vanilla HTML/JS setup wizard for devices (<100KB) ✅ COMPLETE
4. **home_base_dashboard/** - Preact local dashboard ✅ COMPLETE (13.23KB gzipped)
5. Device firmware - ESP32-S3 motion sensors/cameras with ESP-NOW mesh (NOT STARTED)

## Architecture

### Communication Flow
```
ESP32-S3 Devices → ESP-NOW → P4 Home Base → HTTP/REST → Unraid Central API
                             ↓ (local)           ↓ (cloud/LAN)
                        Dashboard UI         Admin Dashboard
```

### Message Protocol (home_base_firmware/main/include/protocol.h)
All ESP-NOW messages use `mesh_message_t` (285 bytes total):
- **Type byte** (0x01=heartbeat, 0x02=motion, 0x03=log, 0x04=command)
- **Device ID** (16 bytes, null-terminated string)
- **Timestamp** (4 bytes, uint32_t Unix time)
- **Payload** (200 bytes, JSON string)
- **Signature** (64 bytes, Ed25519)

**Critical**: Python log ingestion verifies signatures against `"{int(timestamp)}:{message}"` format (see main.py:120-125)

### Security Model
- **Ed25519** for all signing (PyNaCl in Python, TinyCrypt planned for firmware)
- **TOTP** for API authentication (pyotp, Google Authenticator compatible)
- **Session tokens** expire in 15 minutes (see security.py:ACCESS_TOKEN_EXPIRE_MINUTES)
- Network keys derive from master, device keys derive from network keys
- Private keys shown ONLY ONCE at device registration

## Development Workflows

### Unraid API (unraid_api/)
**Run locally:**
```bash
cd unraid_api
python -m venv venv
venv\Scripts\activate  # Windows
pip install -r requirements.txt
python generate_keys.py  # First-time setup: creates admin user
uvicorn main:app --reload
```

**Run in Docker:**
```bash
cd unraid_api
docker-compose up --build
```

**Database**: SQLite at `./data/unraid_central.db` (auto-created on first run)

### Home Base Firmware (home_base_firmware/)
**Prerequisites**: ESP-IDF 5.2+ installed with `idf.py` in PATH

**Build & Flash:**
```bash
cd home_base_firmware
idf.py set-target esp32p4
idf.py menuconfig  # Configure IP101 PHY, DISABLE C6 coprocessor
idf.py build
idf.py -p COM3 flash monitor  # Adjust port
```

**Hardware notes**:
- Use Type-C UART port (NOT USB OTG)
- Ethernet: IP101 PHY with RMII interface (built-in)
- ESP-NOW requires WiFi STA mode even with Ethernet active

## Code Conventions

### Python (unraid_api/) - PRODUCTION READY ✅
- **FastAPI** with SQLAlchemy ORM (models.py defines all tables)
- Database dependency injection via `Depends(get_db)` pattern
- Pydantic models for request/response validation (top of main.py)
- **Auth enforcement**: `middleware.get_current_user` dependency validates Bearer tokens on mutation endpoints
- Signature verification: reconstructs exact `"{int(timestamp)}:{message}"` format that firmware signs
- **24 pytest tests** in `unraid_api/tests/` covering all endpoints with SQLite StaticPool for thread safety

### C Firmware (home_base_firmware/) - PRODUCTION READY ✅
- **ESP-IDF component structure**: main/ contains all application code
- ESP_LOG macros for logging (TAG defined per file)
- **Complete implementations**:
  - `esp_now_mesh.c` - Handles ESP-NOW reception, forwards logs to Unraid via HTTP
  - `http_server.c` - 13 endpoints: status, devices, config portal (10 endpoints), logs, motion, command
  - `unraid_client.c` - Posts mesh messages to `/logs/ingest` endpoint, hex-encodes signatures
  - `log_storage.c` - FIFO storage for 500 logs and 100 motion events
  - `device_config.c` - NVS persistent configuration with JSON serialization
  - `main.c` - Initialization sequence: NVS → Ethernet → ESP-NOW → HTTP Server
- **Critical**: Use `vTaskDelay(pdMS_TO_TICKS(ms))` for delays, never `sleep()`
- **Note**: Signature verification TODO documented in http_server.c (requires network private key)

### Device Config Portal (device_config_portal/) - COMPLETE ✅
- **Vanilla HTML/JS** (no dependencies, 45KB total)
- 5-step wizard: Device Type → WiFi → Network → Sensors → Review
- Runs on device AP at `http://192.168.4.1` when device is unconfigured
- **Integration requirement**: Firmware must implement the 10 REST endpoints this portal calls
- See [device_config_portal/FIRMWARE_INTEGRATION.md](../device_config_portal/FIRMWARE_INTEGRATION.md) for API contract
- Ready for production deployment - just needs firmware to handle the endpoints

## Key Files to Reference

### Backend API
- [unraid_api/main.py](../unraid_api/main.py) - All API endpoints
- [unraid_api/security.py](../unraid_api/security.py) - Ed25519 verification, TOTP, JWT
- [unraid_api/models.py](../unraid_api/models.py) - Database schema (6 tables: users, networks, devices, device_logs, motion_events, firmware)

### Firmware
- [home_base_firmware/main/include/protocol.h](../home_base_firmware/main/include/protocol.h) - ESP-NOW message format
- [home_base_firmware/main/main.c](../home_base_firmware/main/main.c) - Initialization sequence
- [home_base_firmware/main/esp_now_mesh.c](../home_base_firmware/main/esp_now_mesh.c) - Mesh message reception

### Documentation
- [WORKSTREAM_A_BACKEND.md](../WORKSTREAM_A_BACKEND.md) - Backend API spec with all endpoints
- [WORKSTREAM_B_FRONTEND.md](../WORKSTREAM_B_FRONTEND.md) - Frontend requirements (3 UIs: device config, home base dashboard, admin dashboard)
- [WORKSTREAM_C_FIRMWARE.md](../WORKSTREAM_C_FIRMWARE.md) - Device firmware spec with hardware details

## Common Tasks

### Adding an API Endpoint
1. Define Pydantic request/response models at top of main.py
2. Add route with `@app.post/get("/path")` decorator
3. Use `db: Session = Depends(get_db)` for database access
4. Update WORKSTREAM_A_BACKEND.md with endpoint documentation

### Adding ESP-NOW Message Type
1. Add `#define MSG_TYPE_<NAME>` in protocol.h
2. Handle in `OnDataRecv` callback (esp_now_mesh.c)
3. Update Python ingestion logic if backend needs to process it
4. Document in WORKSTREAM_A_BACKEND.md and WORKSTREAM_C_FIRMWARE.md

### Signature Verification Debugging
**Python side**: Message format MUST match firmware exactly. Current format is `"{int(timestamp)}:{message}"`.
Check main.py:120-125 for verification logic. Enable debug prints to see raw message being verified.

**Firmware side**: Not yet implemented (planned with TinyCrypt). Will need to sign same format.

## Priority Items

### ✅ Authentication Implementation (COMPLETED)
All admin endpoints now enforce session token auth via `middleware.get_current_user`:
- POST /networks requires token ✅
- POST /networks/{id}/devices requires token ✅
- POST /networks/{id}/command requires token ✅
- GET endpoints (list operations) do NOT require auth ✅
- Token refresh and logout: see security.py for implementation details

### ⏳ Home Base Firmware Configuration Endpoints (NEXT)
Implement 10 device config portal endpoints in firmware:
1. GET /api/wifi/scan - returns available WiFi networks
2. POST /api/device/set-type - save device type (motion/camera)
3. POST /api/wifi/connect - connect to selected WiFi
4. POST /api/device/register - save network ID and private key
5. POST /api/config/sensors - PIR GPIO, sensitivity, cooldown
6. POST /api/config/led - LED GPIO, colors, brightness
7. POST /api/config/camera - camera SPI pins, resolution
8. POST /api/config/hardware - board variant, GPIO auto-detect
9. POST /api/reboot - trigger device restart
10. GET /api/device/type - retrieve current device type

See "Hardware Configuration via Web UI" section for full endpoint specs.
All config persisted to NVS as JSON (see protocol.h comments for format).

### 🚀 Device Firmware Development (FUTURE)
When starting device firmware work:
1. Create new component `firmware/device_firmware/` for ESP32-S3 variants
2. Implement NVS config storage with JSON serialization (see Workstream C)
3. Add WiFi + AP mode with fallback to config AP at `192.168.4.1`
4. Implement device config portal (from Workstream B frontend, <100KB)
5. Add PIR sensor with GPIO interrupt (configurable sensitivity 1-10, cooldown 5-300s)
6. Implement Ed25519 signing with TinyCrypt (match Python format exactly: `"{int(timestamp)}:{message}"`)
7. Create test suite in `firmware/device_firmware/test/` following ESP-NOW test patterns

## Testing Strategy

### Backend Tests (Python) - 24 TESTS ✅ COMPLETE
All tests located in `unraid_api/tests/` following pytest patterns with FastAPI TestClient:
- **conftest.py** (7 fixtures): Database setup with StaticPool, test client, TOTP generators, keypair factories
- **test_auth.py** (5 tests): Login success/failure, TOTP validation, token expiry
- **test_networks.py** (6 tests): Create network, list networks, register device, list devices
- **test_logs.py** (7 tests): Log ingestion with valid/invalid signatures, filtering, database persistence
- **test_commands.py** (6 tests): Command signing, nonce validation, replay attack prevention

**Run tests:**
```bash
cd unraid_api
pytest                              # Run all 24
pytest tests/test_logs.py -v       # Signature verification tests
pytest -k "signature" --tb=short   # Focus on critical path
```

**Critical Pattern**: Message signature format MUST be `"{int(timestamp)}:{message}"` - this is verified in test_logs.py against Python PyNaCl and must match firmware exactly.

### Firmware Tests (C) - 11 TESTS ✅ COMPLETE
Located in `home_base_firmware/test/` using ESP-IDF's Unity test framework:
- **test_esp_now_mesh.c** (6 tests): Message reception, type parsing, log forwarding
- **test_http_server.c** (5 tests): Status endpoint, device list, response format

**Run tests:**
```bash
cd home_base_firmware
idf.py set-target esp32p4
idf.py build
idf.py -p COM3 flash monitor
```

**Test Patterns Reference:**
- See `unraid_api/tests/test_logs.py` for signature format examples
- See `home_base_firmware/test/` for cJSON parsing patterns
- See `home_base_firmware/main/unraid_client.c` for hex signature encoding (critical)

## Hardware Configuration via Web UI

**CRITICAL**: GPIO pins, sensor configurations, and camera setup are ALL user-configurable via web UI, NOT hardcoded.

### Device Config Portal (Device AP Mode)
Served automatically at `http://192.168.4.1` when device has no WiFi config (AP "ESP32-MESH-{MAC}").

**Step 1: Device Type Selection**
```
GET /api/device/type
  Response: {"type": "camera|motion"}  // Persisted in NVS

POST /api/device/set-type
  Request: {"type": "motion"}  // 0x01 or 0x02
  Response: {"status": "saved"}
```

**Step 2: WiFi Configuration**
```
GET /api/wifi/scan
  Response: [
    {"ssid": "network1", "rssi": -45, "security": "WPA2"},
    {"ssid": "network2", "rssi": -60, "security": "open"}
  ]

POST /api/wifi/connect
  Request: {"ssid": "network1", "password": "password123"}
  Response: {"status": "connecting", "retry_count": 3}
```

**Step 3: Device Registration**
```
POST /api/device/register
  Request: {
    "device_id": "ESP32-ABC123",
    "network_id": 1,
    "private_key": "hex_key_from_unraid"
  }
  Response: {"status": "registered"}
```

**Step 4: Sensor Configuration** (shown based on device type)
```
POST /api/config/sensors
  Request: {
    "pir_gpio": 39,                 // GPIO pin for PIR sensor
    "pir_sensitivity": 5,            // 1-10 (higher = more sensitive)
    "pir_cooldown_ms": 30000,        // 5000-300000ms
    "pir_invert_logic": false        // Some sensors are active-low
  }
  Response: {"status": "saved", "reboot_in_seconds": 3}
```

**Step 5: LED Configuration** (if RGB LED present)
```
POST /api/config/led
  Request: {
    "led_gpio": 48,                  // WS2812 data pin
    "led_brightness": 80,            // 0-100%
    "led_mode": "motion_reactive",   // or "always_on"
    "color_clear": "00FF00",         // Green
    "color_cooldown": "FFFF00",      // Yellow
    "color_motion": "FF0000"         // Red
  }
  Response: {"status": "saved"}
```

**Step 6: Camera Configuration** (if camera present)
```
POST /api/config/camera
  Request: {
    "camera_enable": true,
    "camera_resolution": "VGA",      // QVGA/VGA/SVGA
    "camera_capture_mode": "burst",  // single/burst/video
    "camera_frames": 5,              // For burst mode
    "camera_sclk_gpio": 8,           // SPI clock
    "camera_mosi_gpio": 13,          // SPI MOSI
    "camera_miso_gpio": 11,          // SPI MISO
    "camera_cs_gpio": 12,            // SPI chip select
    "camera_pwdn_gpio": 47           // Power down control
  }
  Response: {"status": "saved"}
```

**Step 7: Hardware Mapping** (board variant selection)
```
POST /api/config/hardware
  Request: {
    "board_variant": "esp32s3_devkitm",  // or freenove_cam, xiao_s3, etc
    "auto_detect_gpio": true              // Auto-detect based on board
  }
  Response: {
    "status": "saved",
    "detected_gpios": {
      "pir_gpio": 39,
      "led_gpio": 48,
      "sd_clk": 14,
      "sd_cmd": 7
    }
  }
```

### Configuration Storage in NVS
All settings persisted to NVS with key `"device_config"` as JSON:
```c
{
  "device_id": "ESP32-ABC123",
  "network_id": 1,
  "type": "motion",
  "private_key": "hex_encoded",
  "pir_gpio": 39,
  "pir_sensitivity": 5,
  "pir_cooldown_ms": 30000,
  "led_gpio": 48,
  "led_brightness": 80,
  "camera_enabled": true,
  "board_variant": "esp32s3_devkitm"
}
```

### Critical Implementation Notes
1. **GPIO pins are NOT constants** - Always read from NVS config, never hardcode
2. **Sensitivity is 1-10 scale** - Map to actual ADC threshold or interrupt delay
3. **Cooldown prevents false positives** - Ignore PIR triggers within cooldown window
4. **Board auto-detection** - If user selects board variant, pre-populate GPIO defaults
5. **Reboot after save** - Most config changes require device reboot (shown in UI)
6. **Backward compatibility** - If NVS config missing, show config portal automatically

### Config Portal Endpoints Summary
```
API Endpoints (Device Config Portal)
GET /api/wifi/scan
GET /api/device/type
POST /api/device/set-type
POST /api/wifi/connect
POST /api/device/register
POST /api/config/sensors
POST /api/config/led
POST /api/config/camera
POST /api/config/hardware
POST /api/reboot
```

All settings use NVS as persistent storage. Frontend (Workstream B) builds UI around these endpoints.
