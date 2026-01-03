# ESP32-C6 Motion Sensor Device Firmware

Complete firmware implementation for ESP32-C6 motion sensor with ST7789 TFT display, ESP-NOW mesh integration, and configurable GPIO pins.

## Quick Start

### Prerequisites
- ESP-IDF 5.2 or later
- ESP32-C6-DevKit or equivalent board
- ST7789 TFT display (240x320)
- PIR motion sensor
- Build tools: cmake, ninja (or make)

### Build & Flash
```bash
cd device_firmware/esp32c6_motion
idf.py set-target esp32c6
idf.py menuconfig  # Configure GPIO pins and display settings
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

### Initial Setup
1. Device starts in **AP mode** if unconfigured
2. Connect to WiFi SSID: `ESP32-MESH-XXXXXX`
3. Open browser: `http://192.168.4.1`
4. Configure device via web portal (from device_config_portal)
5. Device reboots and enters **STA mode** with ESP-NOW enabled

## Architecture

### Component Structure
```
esp32c6_motion/
├── main.c                    # Application entry point
├── device_config.c/h         # NVS configuration persistence
├── display_driver.c/h        # ST7789 SPI driver
├── display_ui.c/h           # High-level display UI
├── motion_sensor.c/h        # PIR sensor with GPIO interrupt
├── http_server.c/h          # AP mode configuration endpoints
├── esp_now_device.c/h       # Mesh client integration
├── include/                 # Header files
├── CMakeLists.txt          # Build configuration
├── Kconfig.projbuild       # Configuration options
└── README.md               # This file
```

### Initialization Sequence
```
app_main()
├── device_config_init()      [Load NVS settings]
├── device_config_load()      [Restore from flash]
├── display_init()            [SPI + ST7789 setup]
├── motion_sensor_init()      [GPIO interrupt + queue]
├── init_wifi()               [AP or STA mode]
├── http_server_start()       [Configuration endpoints]
├── esp_now_device_init()     [Mesh if configured]
└── main_event_loop()         [Display updates + heartbeats]
```

## Components

### 1. Device Configuration (device_config.c/h)

**Purpose**: Persistent NVS storage with JSON serialization

**Configuration Structure**:
```c
typedef struct {
    char device_id[32];                 // Unique device identifier
    int network_id;                     // Home base network ID
    uint8_t type;                       // Device type (0x02 = motion)
    
    // Motion sensor
    uint8_t motion_gpio;                // GPIO pin for PIR
    uint8_t motion_sensitivity;         // 1-10 scale
    uint32_t motion_cooldown_ms;        // 5000-300000ms
    
    // Display (ST7789)
    bool display_enabled;
    uint16_t display_width;             // 240 pixels
    uint16_t display_height;            // 320 pixels
    uint8_t display_sclk_gpio;          // SPI clock
    uint8_t display_mosi_gpio;          // SPI MOSI
    uint8_t display_miso_gpio;          // SPI MISO
    uint8_t display_cs_gpio;            // Chip select
    uint8_t display_dc_gpio;            // Data/Command
    uint8_t display_reset_gpio;         // Reset line
    uint8_t display_backlight_gpio;     // Backlight
    uint8_t display_brightness;         // 0-100%
    
    // Colors (RGB565)
    uint16_t color_motion;              // Motion detected color
    uint16_t color_clear;               // No motion color
    uint16_t color_cooldown;            // Cooldown phase color
    uint16_t color_text;                // Text color
    uint16_t color_background;          // Background color
    
    char board_variant[32];             // "esp32c6"
} device_config_t;
```

**NVS Storage**:
- Namespace: `device`
- Key: `config`
- Format: JSON string (auto-formatted for readability)
- Max size: 1024 bytes

**Key Functions**:
- `device_config_init()` - Initialize NVS flash
- `device_config_load()` - Load from NVS or use defaults
- `device_config_save()` - Persist to NVS
- `device_config_is_configured()` - Check if device registered

### 2. Display Driver (display_driver.c/h)

**Purpose**: ST7789 TFT controller communication via SPI3

**Features**:
- 240x320 pixel display
- 16-bit RGB565 color
- SPI interface at 40MHz
- Configurable GPIO pins from NVS

**Key Functions**:
- `display_init()` - Initialize SPI and ST7789
- `display_fill_color()` - Fill entire screen
- `display_set_window()` - Set drawing region
- `display_draw_pixel()` - Plot single pixel
- `display_set_brightness()` - Control backlight

**SPI Configuration**:
```c
SCLK_GPIO = 6    (configurable)
MOSI_GPIO = 7    (configurable)
MISO_GPIO = 5    (configurable)
CS_GPIO = 8      (configurable)
DC_GPIO = 9      (configurable)
RESET_GPIO = 10  (configurable)
BL_GPIO = 11     (configurable)
```

**ST7789 Commands Used**:
- `0x01` - Software Reset (SWRESET)
- `0x11` - Sleep Out (SLPOUT)
- `0x3A` - Color Mode (16-bit/pixel)
- `0x2A` - Set Column (CASET)
- `0x2B` - Set Row (RASET)
- `0x2C` - Write RAM (RAMWR)
- `0x29` - Display ON (DISPON)

### 3. Motion Sensor (motion_sensor.c/h)

**Purpose**: PIR motion detection with cooldown debouncing

**Features**:
- GPIO interrupt on rising edge
- Configurable cooldown to prevent false positives
- Event queue for async processing
- Callback support

**Configuration**:
```c
motion_gpio = 4                 // GPIO pin
motion_sensitivity = 5          // 1-10 scale
motion_cooldown_ms = 30000      // 30 seconds default
```

**Key Functions**:
- `motion_sensor_init()` - Install GPIO ISR
- `motion_sensor_start_task()` - Begin processing events
- `motion_sensor_is_motion_detected()` - Current state
- `motion_sensor_time_since_motion()` - Milliseconds elapsed
- `motion_sensor_set_sensitivity()` - Update sensitivity
- `motion_sensor_set_cooldown()` - Update cooldown

**Event Structure**:
```c
typedef struct {
    uint32_t timestamp;      // When detected
    bool motion_detected;    // True/false
    uint8_t gpio;           // Which GPIO triggered
} motion_event_t;
```

### 4. Display UI (display_ui.c/h)

**Purpose**: High-level display rendering with motion status

**Features**:
- Show motion detection status
- Display device network info
- Error and connecting screens
- Color-coded status indication

**Status Display**:
- **Motion Detected** → `color_motion` (default: red)
- **Cooldown** → `color_cooldown` (default: yellow)
- **Clear** → `color_clear` (default: green)

**Key Functions**:
- `display_ui_show_motion_status()` - Update main status screen
- `display_ui_show_splash()` - Splash screen on boot
- `display_ui_show_network_info()` - Show device details
- `display_ui_show_error()` - Error message
- `display_ui_show_connecting()` - Status message

### 5. HTTP Configuration Server (http_server.c/h)

**Purpose**: AP mode endpoints for device setup (matches device_config_portal contract)

**Endpoints** (all JSON):

| Method | Path | Purpose |
|--------|------|---------|
| GET | `/api/wifi/scan` | List available WiFi networks |
| POST | `/api/config/motion` | Update motion sensor config |
| POST | `/api/config/display` | Update display settings |
| POST | `/api/device/register` | Register with network ID |
| GET | `/api/device/type` | Get device type |
| GET | `/api/v1/status` | Device status overview |
| POST | `/api/reboot` | Restart device |

**Example Requests**:

```bash
# Scan WiFi
curl http://192.168.4.1/api/wifi/scan

# Configure motion sensor
curl -X POST http://192.168.4.1/api/config/motion \
  -H "Content-Type: application/json" \
  -d '{
    "motion_gpio": 4,
    "motion_sensitivity": 5,
    "motion_cooldown_ms": 30000
  }'

# Configure display
curl -X POST http://192.168.4.1/api/config/display \
  -H "Content-Type: application/json" \
  -d '{
    "brightness": 100,
    "color_motion": 63488,
    "color_clear": 2016,
    "color_cooldown": 65504,
    "color_text": 65535
  }'

# Register device
curl -X POST http://192.168.4.1/api/device/register \
  -H "Content-Type: application/json" \
  -d '{
    "device_id": "ESP32-C6-MOTION-001",
    "network_id": 1
  }'

# Reboot
curl -X POST http://192.168.4.1/api/reboot
```

### 6. ESP-NOW Device Client (esp_now_device.c/h)

**Purpose**: Mesh communication with home base

**Features**:
- Send motion events to home base
- Receive commands from home base
- Message queue for async processing
- Ed25519 signature support (prepared)

**Message Format** (from protocol.h):
```c
typedef struct {
    uint8_t type;           // 0x01=heartbeat, 0x02=motion, etc.
    char device_id[16];     // Null-terminated
    uint32_t timestamp;     // Unix seconds
    char payload[200];      // JSON data
    uint8_t signature[64];  // Ed25519 signature
} mesh_message_t;  // Total: 285 bytes
```

**Signature Format** (matching Python backend):
```
Message to sign: "{int(timestamp)}:{motion_event}"
Example: "1234567890:{"motion":true,"sensitivity":5}"
Signed with: Device private key (Ed25519)
```

**Key Functions**:
- `esp_now_device_init()` - Initialize WiFi + ESP-NOW
- `esp_now_device_add_peer()` - Register home base
- `esp_now_device_send_motion_event()` - Send motion detection
- `esp_now_device_send_heartbeat()` - Health check
- `esp_now_device_process_messages()` - Handle received messages

### 7. Main Application (main.c)

**Purpose**: Orchestration and event loop

**Initialization**:
1. Load configuration from NVS
2. Initialize display (optional)
3. Initialize motion sensor
4. Start WiFi (AP or STA)
5. Start HTTP server
6. Initialize ESP-NOW (if configured)
7. Start event loop

**Main Event Loop**:
- Update display every 500ms
- Send heartbeat every 30 seconds
- Process ESP-NOW messages continuously
- Monitor motion sensor state

## Configuration

### Via menuconfig
```bash
idf.py menuconfig
# Navigate to: ESP32-C6 Motion Sensor Configuration
#   - Motion Sensor GPIO Pin (default: 4)
#   - Motion Sensor Sensitivity (1-10, default: 5)
#   - Motion Cooldown Period (ms, default: 30000)
#   - Display settings (width, height, GPIO pins)
```

### Via HTTP (when unconfigured)
Device listens at `http://192.168.4.1` with 7 endpoints for full configuration.

### Via ESP-NOW (future)
Home base can send configuration commands to device via ESP-NOW.

## Build & Deployment

### Build for ESP32-C6
```bash
cd device_firmware/esp32c6_motion
idf.py set-target esp32c6
idf.py build
```

### Flash to Device
```bash
idf.py -p /dev/ttyACM0 flash monitor

# Adjust port:
#   Linux: /dev/ttyACM0 or /dev/ttyUSB0
#   macOS: /dev/tty.usbserial-*
#   Windows: COM3 (or appropriate COM port)
```

### Monitor Logs
```bash
idf.py monitor           # While flashing
idf.py monitor -p COM3   # After flashing (separate terminal)
```

## Default Configuration

**Motion Sensor**:
```
GPIO: 4
Sensitivity: 5/10
Cooldown: 30 seconds
```

**Display (ST7789)**:
```
Resolution: 240x320 pixels
SPI Clock: 40 MHz
SPI Pins: SCLK=6, MOSI=7, MISO=5, CS=8
Control: DC=9, RESET=10, BL=11
Brightness: 100%
```

**Colors (RGB565)**:
```
Motion Detected: 0xF800 (Red)
Clear: 0x07E0 (Green)
Cooldown: 0xFFE0 (Yellow)
Text: 0xFFFF (White)
Background: 0x0000 (Black)
```

## Integration Points

### With Home Base Firmware
- Sends motion events via ESP-NOW
- Receives commands via ESP-NOW
- Message format: `mesh_message_t` (285 bytes)
- Signature format: `"{timestamp}:{message}"` (Ed25519)

### With Device Config Portal
- Implements all 7 required HTTP endpoints
- Saves configuration to NVS
- Restarts when configuration complete

### With Unraid Central API
- Home base forwards device logs to `/logs/ingest`
- Motion events stored in `motion_events` table
- Device status visible in Central dashboard

## Memory Usage

| Component | Size |
|-----------|------|
| Flash (code) | ~200KB |
| IRAM (ISR) | ~8KB |
| DRAM (heap) | ~100KB |
| NVS (config) | ~2KB |

**Typical Heap Usage**: 40-50% available for logs/buffers

## Power Considerations

- **Active mode**: ~80mA (WiFi + display)
- **Motion detection**: GPIO interrupt (minimal power)
- **Display update**: 500ms refresh cycle
- **Heartbeat**: Every 30 seconds (low power transmission)

Future enhancements could add deep sleep for battery operation.

## Troubleshooting

### Display not initialized
- Check GPIO pins in menuconfig match your board
- Verify SPI clock frequency (40MHz)
- Ensure ST7789 is receiving power

### Motion sensor not detecting
- Verify GPIO pin in menuconfig
- Check PIR sensor wiring (3.3V, GND, Signal)
- Test with GPIO interrupt pin pull-ups

### HTTP server not accessible
- Device in AP mode? Check WiFi SSID `ESP32-MESH-XXXXXX`
- WiFi not configured? Device defaults to AP mode
- IP address: `http://192.168.4.1`

### ESP-NOW communication fails
- Device must be registered (network_id > 0)
- Home base MAC address must be added as peer
- Ensure both devices on same WiFi channel

## Testing

### Manual Testing
1. Flash device
2. Connect to AP
3. Access `http://192.168.4.1`
4. Configure motion GPIO and display colors
5. Watch display update with motion status

### Automated Testing (future)
- Unit tests for device_config (NVS persistence)
- Integration tests for display_driver
- End-to-end tests with ESP-NOW communication

## Future Enhancements

1. **PWM brightness control** - Replace GPIO on/off with PWM
2. **Font rendering** - Add proper text display on TFT
3. **SD card logging** - Store motion events to local storage
4. **Battery operation** - Deep sleep between motion events
5. **OTA updates** - Receive firmware updates from home base
6. **MQTT integration** - Direct cloud connectivity
7. **Gesture control** - Swipe display for menu navigation
8. **Time synchronization** - NTP for accurate timestamps

## Related Documentation

- [device_config_portal/](../../device_config_portal/) - Web UI for setup
- [home_base_firmware/](../home_base_firmware/) - Mesh coordinator
- [unraid_api/](../../unraid_api/) - Central dashboard API
- [WORKSTREAM_C_FIRMWARE.md](../../WORKSTREAM_C_FIRMWARE.md) - Device requirements
