# Home Base P4-ETH Firmware

ESP32-P4-ETH-M coordinator firmware for the E32 Mesh Backend system. This firmware coordinates ESP-NOW mesh messages from remote devices and forwards them to the Unraid Central API.

## Features

✅ **Ethernet Connectivity** - IP101 PHY with built-in Ethernet
✅ **ESP-NOW Mesh Coordinator** - Receives messages from ESP32-S3 devices
✅ **Device Configuration Portal** - Web-based setup wizard at `http://192.168.4.1`
✅ **REST API** - Status, device listing, and configuration endpoints
✅ **NVS Configuration** - Persistent device configuration storage
✅ **Log Forwarding** - HTTP client for sending logs to Unraid API
✅ **Production-Ready** - Comprehensive error handling and logging

## Hardware

- **Chip**: ESP32-P4-ETH-M
- **Ethernet**: IP101 PHY with RMII interface (built-in)
- **Ports**: Type-C UART (NOT USB OTG)
- **Power**: 5V USB-C

## Prerequisites

1. **ESP-IDF 5.2+** installed with `idf.py` in PATH
   ```bash
   cd ~/esp/esp-idf
   export IDF_PATH=$(pwd)
   . ./export.sh
   ```

2. **Python dependencies**:
   ```bash
   pip install pyserial
   ```

## Building & Flashing

### Quick Start

```bash
cd home_base_firmware

# Configure for P4
idf.py set-target esp32p4

# Configure Ethernet and API URL
idf.py menuconfig
# Navigate to: Home Base Configuration → Unraid API URL
# Update if your Unraid is on a different IP

# Build
idf.py build

# Flash (hold BOOT, press RESET, release BOOT before flashing)
idf.py -p /dev/ttyACM0 flash monitor
```

### Menuconfig Options

**Home Base Configuration** section provides:

- **Unraid API URL** - Default: `http://192.168.1.100:8000/logs/ingest`
- **Ethernet PHY Address** - Default: 1 (IP101)
- **ESP-NOW Channel** - Default: 1
- **HTTP Server Port** - Default: 80
- **Device Config Portal** - Enable/disable config portal

### Build Variants

```bash
# Clean build
idf.py clean

# Verbose output
idf.py build -v

# Size report
idf.py size

# Monitor logs only (already flashed)
idf.py monitor -p /dev/ttyACM0
```

## Architecture

### Initialization Sequence

```
app_main()
  ├─ device_config_init()     → NVS initialization
  ├─ device_config_load()     → Load from NVS or use defaults
  ├─ esp_netif_init()         → Network interface
  ├─ esp_event_loop_create()  → Event handling
  ├─ init_ethernet()          → IP101 PHY setup
  ├─ init_esp_now()           → ESP-NOW mesh with WiFi STA mode
  └─ start_webserver()        → HTTP server for config portal + API
```

### Component Organization

| File | Purpose |
|------|---------|
| `main.c` | App entry point, initialization sequence |
| `device_config.c` | NVS configuration management with JSON serialization |
| `esp_now_mesh.c` | ESP-NOW message reception and routing |
| `http_server.c` | HTTP endpoints (status, device config, etc.) |
| `unraid_client.c` | HTTP client for forwarding logs to Unraid |
| `protocol.h` | Message format definition (mesh_message_t) |

## API Endpoints

### Status Endpoints

```
GET /api/v1/status
  Response: {"status": "online", "role": "home_base", "device_id": "...", "network_id": 1}

GET /api/v1/devices
  Response: [{"id": "dev1", "status": "online"}]
```

### Device Configuration Portal Endpoints

Implemented for device setup wizard (device_config_portal/index.html):

```
GET  /api/device/type              → Retrieve current device type
POST /api/device/set-type          → Set device type (motion/camera)
GET  /api/wifi/scan                → Scan available WiFi networks
POST /api/wifi/connect             → Connect to WiFi
POST /api/device/register          → Register with network ID
POST /api/config/sensors           → Configure PIR sensor (GPIO, sensitivity, cooldown)
POST /api/config/led               → Configure LED (GPIO, brightness, colors)
POST /api/config/camera            → Configure camera (resolution, SPI pins)
POST /api/config/hardware          → Set board variant and GPIO auto-detect
POST /api/reboot                   → Trigger device restart
```

## Configuration Management

### NVS Storage

All device configuration is persisted to NVS namespace `"device"` with key `"config"` as JSON:

```json
{
  "device_id": "ESP32-P4-ABC123",
  "network_id": 1,
  "type": "motion",
  "pir_gpio": 39,
  "pir_sensitivity": 5,
  "pir_cooldown_ms": 30000,
  "led_gpio": 48,
  "led_brightness": 80,
  "camera_enabled": false,
  "board_variant": "esp32p4_eth"
}
```

### Default Values

If not configured or NVS is erased:

```c
device_id = "ESP32-P4-UNCONFIGURED"
network_id = 0  // Indicates unconfigured state
type = 0
pir_gpio = 39
pir_sensitivity = 5
pir_cooldown_ms = 30000
led_gpio = 48
led_brightness = 80
camera_enabled = false
board_variant = "esp32p4_eth"
```

## Message Flow

### ESP-NOW Reception

1. Remote device sends `mesh_message_t` (285 bytes)
2. `OnDataRecv()` callback queues message
3. `mesh_processing_task` routes by message type:
   - `MSG_TYPE_HEARTBEAT` - Update device status
   - `MSG_TYPE_MOTION` - Send to Unraid via HTTP
   - `MSG_TYPE_LOG` - Send to Unraid via HTTP
   - `MSG_TYPE_COMMAND` - Execute command (validate signature)

### Log Forwarding to Unraid

```c
mesh_message_t → send_log_to_unraid()
  ├─ Serialize to LogIngestRequest JSON
  ├─ Hex-encode Ed25519 signature
  ├─ POST to /logs/ingest endpoint
  └─ Parse HTTP response (200 OK expected)
```

## Testing

### Local Testing

Run the device status endpoint:

```bash
# From any computer on the same network
curl http://<P4-IP>/api/v1/status
# Response: {"status":"online","role":"home_base","device_id":"...","network_id":1}
```

### ESP-IDF Unit Tests

Located in `test/` directory:

```bash
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

Tests cover:
- ESP-NOW message reception
- HTTP server responses
- Message type parsing

## Debugging

### Serial Monitor

```bash
idf.py monitor -p /dev/ttyACM0
```

Key log tags:
- `home_base` - Main initialization and status
- `esp_now` - ESP-NOW mesh messages
- `http_server` - HTTP endpoints
- `device_config` - Configuration loading/saving
- `unraid_client` - Log forwarding

### Common Issues

**Issue**: Ethernet not connecting
- **Solution**: Ensure IP101 PHY is properly configured in menuconfig
- **Check**: PHY address (default 1), RMII pins configured

**Issue**: ESP-NOW not receiving messages
- **Solution**: Ensure WiFi STA mode is initialized before ESP-NOW
- **Check**: `init_esp_now()` called after WiFi init, correct channel configured

**Issue**: HTTP server not responding
- **Solution**: Check port not already in use
- **Check**: Firewall allowing port 80 or configured port

**Issue**: Device config not persisting
- **Solution**: Erase NVS and reconfigure
- **Command**: `idf.py erase-flash`

## Performance

- **Message latency**: ESP-NOW → HTTP forward ~50-100ms typical
- **Queue capacity**: 20 messages (MESH_QUEUE_SIZE)
- **HTTP timeout**: 5 seconds per request
- **Concurrent connections**: Limited by HTTPD configuration (default 10)

## Security Notes

1. **Ed25519 Signatures**: All ESP-NOW messages must be signed
   - Python backend verifies against `"{int(timestamp)}:{message}"` format
   - Firmware must match exact signing format

2. **TOTP Authentication**: Unraid API requires TOTP for admin endpoints
   - Configure in Unraid dashboard with authenticator app

3. **Session Tokens**: 15-minute expiry
   - Implement token refresh for long-lived operations

## Next Steps

After flashing:

1. **Device not configured**?
   - Device starts AP mode: `ESP32-MESH-{MAC}`
   - Connect to AP and open `http://192.168.4.1`
   - Complete setup wizard

2. **Verify connectivity**:
   - Check `/api/v1/status` returns online
   - Monitor logs for "Ethernet Link Up"

3. **Register devices**:
   - ESP-NOW devices will appear in `/api/v1/devices` after pairing
   - View logs at Unraid Central dashboard

## Building Device Firmware

Once home base is operational:

1. Create device firmware for ESP32-S3 variants
2. Reference `WORKSTREAM_C_FIRMWARE.md` for specifications
3. Implement PIR sensor, LED control, camera support
4. Use this home base firmware as reference for message format and signature verification

## Useful Commands

```bash
# View current config
idf.py read-protect

# Erase NVS only
idf.py erase-region nvs

# Erase all flash
idf.py erase-flash

# Build with verbose output
idf.py build -v

# Generate binary for OTA
idf.py build

# Check code size
idf.py size-components

# View partition table
idf.py partition-table
```

## References

- [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [ESP-NOW Protocol](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_now.html)
- [Protocol Format](./include/protocol.h)
- [Device Config Spec](../device_config_portal/FIRMWARE_INTEGRATION.md)
- [Unraid API Endpoints](../WORKSTREAM_A_BACKEND.md)

## License

Part of E32 Mesh Backend project. See root README for details.
