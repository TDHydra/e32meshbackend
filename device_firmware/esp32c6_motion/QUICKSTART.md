# ESP32-C6 Motion Sensor - Quick Start Guide

Get the device firmware running in 10 minutes.

## 1. Prerequisites (2 minutes)

### Hardware
- ESP32-C6-DevKit (or equivalent)
- ST7789 TFT display (240x320)
- PIR motion sensor
- USB-C cable for flashing

### Software
```bash
# Install ESP-IDF 5.2+
curl -LO https://github.com/espressif/esp-idf/releases/download/v5.2.0/esp-idf-tools-setup-online-5.2.0.exe
# Follow installer, then:
source ~/esp/esp-idf/export.sh  # Linux/macOS
```

## 2. Clone & Prepare (1 minute)

```bash
cd device_firmware/esp32c6_motion
idf.py set-target esp32c6
```

## 3. Configure (2 minutes)

```bash
idf.py menuconfig
```

Navigate to: **ESP32-C6 Motion Sensor Configuration**

Set GPIO pins to match your board:
- Motion GPIO: `4` (default)
- Display SCLK: `6` (default)
- Display MOSI: `7` (default)
- Display MISO: `5` (default)
- Display CS: `8` (default)
- Display DC: `9` (default)
- Display RESET: `10` (default)
- Display BL: `11` (default)

**Save & Exit** (Ctrl+S, Ctrl+Q)

## 4. Build (3 minutes)

```bash
idf.py build
```

Expected output:
```
...
Memory Summary:
  Flash code      : 198KB (16% of 1.25MB)
  Static DRAM     : 48KB (3% of 1.28MB)
  Build complete!
```

## 5. Flash (1 minute)

Connect device via USB-C and flash:

```bash
idf.py -p /dev/ttyACM0 flash monitor
```

**Port notes:**
- Linux: `/dev/ttyACM0` or `/dev/ttyUSB0`
- macOS: `/dev/tty.usbserial-*`
- Windows: `COM3` (adjust as needed)

Expected boot message:
```
I (0) cpu_start: ESP-IDF v5.2.x
I (31) main: === ESP32-C6 Motion Sensor Firmware Start ===
I (35) device_config: Loaded config: device_id=ESP32-C6-UNCONFIGURED
I (45) motion: Motion sensor initialized
I (55) display: ST7789 display initialized (240x320)
I (65) http_server: HTTP server started
I (75) main: Visit http://192.168.4.1 to configure device
```

## 6. Initial Configuration (2 minutes)

### Step 1: Connect to AP
```bash
# Look for WiFi SSID: ESP32-MESH-XXXXXX
# Connect to it (no password)
```

### Step 2: Open Portal
```
Open browser: http://192.168.4.1
```

### Step 3: Configure via Web UI
1. Select device type (Motion Sensor)
2. Scan WiFi networks
3. Enter your WiFi credentials
4. Set Motion GPIO: `4`
5. Set Motion Sensitivity: `5`
6. Set Motion Cooldown: `30000ms`
7. Configure display colors (optional)
8. Review and save → Device reboots

### Step 4: Verify
After reboot, device should:
- ✅ Connect to your WiFi
- ✅ Display motion status on TFT
- ✅ Show "No Motion" (green) initially
- ✅ Change to "Motion Detected" (red) when triggered

## 7. Testing (1 minute)

### Test Motion Detection
```bash
# In separate terminal, monitor logs:
idf.py monitor

# Then wave your hand in front of PIR sensor
# Should see in logs:
# I (12345) motion: Motion event callback - motion=1
# Display should turn red
```

### Test Configuration Endpoint
```bash
# Get device status
curl http://192.168.4.1/api/v1/status

# Update motion sensitivity
curl -X POST http://192.168.4.1/api/config/motion \
  -H "Content-Type: application/json" \
  -d '{"motion_sensitivity": 7}'

# Update display brightness
curl -X POST http://192.168.4.1/api/config/display \
  -H "Content-Type: application/json" \
  -d '{"brightness": 75}'
```

## Done! ✅

Your ESP32-C6 motion sensor is now:
- ✅ Running firmware
- ✅ Configured and online
- ✅ Detecting motion
- ✅ Displaying status on TFT

## Next Steps

1. **Integrate with Home Base**
   - Add device as ESP-NOW peer to home base
   - Device will send motion events to home base

2. **Connect to Unraid**
   - Home base forwards logs to Unraid API
   - Motion events appear in central dashboard

3. **Customize**
   - Change GPIO pins via menuconfig
   - Adjust motion sensitivity (1-10 scale)
   - Customize display colors (RGB565)

## Troubleshooting

### Device not in AP mode?
- Make sure you flashed the latest build
- Check menuconfig settings saved correctly
- Try: `idf.py erase_flash` then rebuild & reflash

### HTTP server not responding?
- Verify WiFi is enabled: `esp_wifi_start()`
- Check menuconfig didn't break build
- Monitor logs: `idf.py monitor`

### Display not showing?
- Verify GPIO pins in menuconfig
- Check SPI clock speed (40MHz)
- Ensure ST7789 has power (3.3V)

### Motion sensor not detecting?
- Verify GPIO pin in menuconfig
- Check PIR sensor power (3.3V)
- Ensure wire connections are solid

## Helpful Commands

```bash
# Clean build
idf.py clean

# Monitor without flashing
idf.py monitor -p /dev/ttyACM0

# Erase entire flash
idf.py erase_flash

# View build log
cat build/config.log

# Set specific target
idf.py set-target esp32c6

# Show environment
idf.py info
```

## Architecture Overview

```
Device Firmware (ESP32-C6)
├─ HTTP Server (AP mode)
│  └─ 7 config endpoints
├─ Motion Sensor (GPIO interrupt)
│  └─ Event queue
├─ Display Driver (SPI)
│  └─ TFT ST7789
├─ Configuration (NVS)
│  └─ JSON persistence
└─ ESP-NOW (WiFi STA)
   └─ Mesh communication
```

## Key Files

| File | Purpose |
|------|---------|
| `main.c` | App entry point & event loop |
| `device_config.c/h` | NVS configuration management |
| `motion_sensor.c/h` | GPIO interrupt handling |
| `display_driver.c/h` | ST7789 SPI communication |
| `http_server.c/h` | Web configuration endpoints |
| `esp_now_device.c/h` | Mesh client |

## Documentation

- **README.md** - Full technical documentation
- **IMPLEMENTATION_COMPLETE.md** - Implementation details
- **Kconfig.projbuild** - Configuration options
- **Code comments** - Inline documentation in source

## Support

For issues or questions, see:
1. `README.md` - Comprehensive reference
2. `IMPLEMENTATION_COMPLETE.md` - Technical details
3. Log output from `idf.py monitor`
4. Home base firmware documentation

**Total time: ~10 minutes from zero to working device** ✨
