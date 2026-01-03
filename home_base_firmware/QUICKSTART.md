# Home Base Firmware - Quick Start Guide

## 5-Minute Setup

### Step 1: Prerequisites
```bash
# Install ESP-IDF 5.2+
mkdir -p ~/esp
cd ~/esp
git clone -b release/v5.2 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh
. ./export.sh
```

### Step 2: Build Firmware
```bash
cd home_base_firmware
idf.py set-target esp32p4
idf.py build
```

### Step 3: Flash Device
```bash
# Hold BOOT button, press RESET, release BOOT
idf.py -p /dev/ttyACM0 flash monitor
```

Watch the logs:
```
[home_base] Home Base Initialized Successfully
[home_base] === Ready for ESP-NOW Mesh Messages ===
[esp_eth] Ethernet Link Up
[esp_now] ESP-NOW Initialized in STA mode
```

## Testing the Firmware

### Endpoint 1: Check Status
```bash
# From any device on the network (find P4-ETH IP first)
curl http://<P4-IP>/api/v1/status

# Response:
# {"status":"online","role":"home_base","device_id":"ESP32-P4-ABC123","network_id":1}
```

### Endpoint 2: Configure Device
```bash
# Set device type to motion
curl -X POST http://<P4-IP>/api/device/set-type \
  -H "Content-Type: application/json" \
  -d '{"type": "motion"}'
# {"status":"saved"}

# Scan WiFi networks
curl http://<P4-IP>/api/wifi/scan
# [{"ssid":"my-network","rssi":-45,"security":3},...]
```

### Endpoint 3: Register with Network
```bash
# Register with Unraid network (network_id=1)
curl -X POST http://<P4-IP>/api/device/register \
  -H "Content-Type: application/json" \
  -d '{
    "device_id": "ESP32-P4-ABC123",
    "network_id": 1,
    "private_key": "hex_key_from_unraid"
  }'
# {"status":"registered"}
```

### Endpoint 4: Configure Sensors
```bash
# Configure PIR sensor
curl -X POST http://<P4-IP>/api/config/sensors \
  -H "Content-Type: application/json" \
  -d '{
    "pir_gpio": 39,
    "pir_sensitivity": 5,
    "pir_cooldown_ms": 30000
  }'
# {"status":"saved","reboot_in_seconds":3}
```

## Verify Unraid Integration

### Step 1: Send Test Log
Simulate a message from a device:
```python
import nacl.signing
import nacl.encoding
import requests
import json
import time

# Create test keypair
signing_key = nacl.signing.SigningKey.generate()

# Create log entry matching firmware format
timestamp = int(time.time())
message = "Test log from device"
message_to_sign = f"{timestamp}:{message}"

# Sign with firmware format
signed = signing_key.sign(message_to_sign.encode())
signature_hex = signed.signature.hex()

# Send to Unraid API
data = {
    "logs": [{
        "device_id": "ESP32-TEST",
        "timestamp": timestamp,
        "level": "INFO",
        "category": "test",
        "message": message,
        "signature": signature_hex
    }]
}

response = requests.post(
    "http://localhost:8000/logs/ingest",
    json=data
)
print(response.status_code, response.json())
```

### Step 2: Verify in Unraid
```bash
# Check logs endpoint
curl http://localhost:8000/logs?device_id=ESP32-TEST&limit=10
```

You should see your test log in the database!

## Configuration Management

### View Current Config
```bash
# Check NVS config in firmware logs
idf.py monitor

# Look for:
# [device_config] Loaded config: device_id=ESP32-P4-ABC123, network_id=1
```

### Reset Configuration
```bash
# Erase NVS (device config)
idf.py erase-region nvs
idf.py flash

# Device will reboot with defaults
```

### Custom Unraid API URL
```bash
idf.py menuconfig
# Home Base Configuration → Unraid API URL
# Change to your IP, e.g. http://192.168.1.50:8000/logs/ingest
```

## Message Format Reference

### ESP-NOW Message (285 bytes)
```c
typedef struct {
    uint8_t type;        // 0x01=heartbeat, 0x02=motion, 0x03=log, 0x04=command
    char device_id[16];  // Device identifier
    uint32_t timestamp;  // Unix timestamp
    char payload[200];   // JSON data
    uint8_t signature[64]; // Ed25519 signature
} mesh_message_t;
```

### Signature Format (Critical!)
```
Message to sign: "{int(timestamp)}:{payload}"
Example: "1704268800:Motion detected in room 1"
Signature: Ed25519 (64 bytes, hex-encoded = 128 chars)
```

### LogIngestRequest JSON
```json
{
  "logs": [{
    "device_id": "ESP32-ABC123",
    "timestamp": 1704268800,
    "level": "INFO",
    "category": "motion",
    "message": "Motion detected in room 1",
    "signature": "hex_encoded_signature_128_chars"
  }]
}
```

## Troubleshooting

### Issue: "Ethernet Link Down"
**Solution**: Check IP101 PHY configuration
```bash
idf.py menuconfig
# Check Home Base Configuration → Ethernet PHY Address = 1
```

### Issue: ESP-NOW not receiving messages
**Solution**: Ensure WiFi STA mode is active
```
[esp_wifi] esp_wifi_set_mode: 1
[esp_now] ESP-NOW Initialized in STA mode
```

### Issue: HTTP endpoints not responding
**Solution**: Check IP address and port
```bash
# Find P4 IP
arp-scan --localnet | grep "Espressif"

# Test connectivity
curl -v http://<IP>/api/v1/status
```

### Issue: Logs not reaching Unraid
**Solution**: Verify API URL in menuconfig and Unraid is running
```bash
# Check Unraid API directly
curl http://localhost:8000/logs
```

### Issue: "Config not found in NVS"
**Solution**: This is normal on first boot. Device will use defaults.
```bash
# Configure via HTTP endpoints or erase and reconfigure
idf.py erase-flash
```

## Integration with Device Firmware

Once device firmware is ready:

1. **Set home base MAC address in device config**
   ```c
   // In device firmware
   esp_now_peer_info_t peer_info = {0};
   // ... set peer_info to home base MAC ...
   esp_now_add_peer(&peer_info);
   ```

2. **Send messages with correct format**
   ```c
   mesh_message_t msg = {0};
   msg.type = MSG_TYPE_MOTION;
   strcpy(msg.device_id, "ESP32-DEVICE-001");
   msg.timestamp = time(NULL);
   strcpy(msg.payload, "{\"event\": \"motion\"}");
   // Sign and set signature...
   esp_now_send(home_base_mac, (void*)&msg, sizeof(msg));
   ```

3. **Monitor in home base**
   ```bash
   idf.py monitor
   # [esp_now] Processing message type=0x02 from ESP32-DEVICE-001
   # [unraid_client] Log sent to Unraid: 200 bytes, response: 200
   ```

## Production Checklist

- [ ] Update Unraid API URL in menuconfig
- [ ] Set correct Ethernet PHY address (default 1 for IP101)
- [ ] Configure WiFi credentials if using STA mode
- [ ] Set device_id in NVS (via API endpoint)
- [ ] Register network_id from Unraid Central
- [ ] Test log forwarding with sample message
- [ ] Verify logs appear in Unraid dashboard
- [ ] Set up device firmware for pairing
- [ ] Configure PIR/LED/camera via HTTP endpoints
- [ ] Test motion events end-to-end

## Performance Characteristics

| Metric | Value |
|--------|-------|
| Boot time | 3-5 seconds |
| Ethernet link up | ~2 seconds after boot |
| ESP-NOW message latency | 50-100ms |
| HTTP timeout | 5 seconds |
| Message queue capacity | 20 messages |
| Max concurrent HTTP connections | 10 |
| Heap usage | ~180KB / 320KB available |

## Advanced Configuration

### Multiple Networks (Future)
Currently supports one home base per network. For multiple networks:
1. Create separate P4-ETH for each network
2. Each home base registers independently with Unraid
3. Unraid Central manages all networks

### OTA Firmware Updates (Future)
Implement in device firmware:
1. POST /api/v1/firmware/check → Check for updates
2. POST /api/v1/firmware/download → Download signed binary
3. Device validates signature and applies OTA
4. Auto-rollback if boot fails

### Command Handling (Future)
Home base can receive commands for devices:
1. Unraid API sends command to home base
2. Home base verifies signature
3. Home base forwards via ESP-NOW to device
4. Device executes and sends ACK

## Support & Documentation

- **Firmware Code**: `home_base_firmware/main/`
- **Configuration**: `home_base_firmware/main/Kconfig.projbuild`
- **API Spec**: `WORKSTREAM_A_BACKEND.md`
- **Message Format**: `home_base_firmware/main/include/protocol.h`
- **Device Config Portal**: `device_config_portal/FIRMWARE_INTEGRATION.md`

## Next: Device Firmware

When ready to develop device firmware:
1. Reference this home base implementation
2. Use `protocol.h` for message format
3. Match signature format exactly: `"{int(timestamp)}:{message}"`
4. Follow ESP-NOW pairing procedures
5. Implement PIR/LED/camera as specified in WORKSTREAM_C_FIRMWARE.md
