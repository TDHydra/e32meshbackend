# Home Base Deployment Guide

## Overview

This guide covers the complete deployment process for the E32 Mesh Backend Home Base system, including:
1. **Home Base Firmware** (ESP32-P4-ETH-M coordinator)
2. **Home Base Dashboard** (Preact web UI)
3. **Unraid Central API** (FastAPI backend)

## Prerequisites

### Hardware
- **ESP32-P4-ETH-M** board with Ethernet (IP101 PHY)
- USB Type-C cable for programming (UART port, NOT USB OTG)
- Ethernet cable
- 5V USB-C power supply
- SD card (optional, for dashboard hosting)

### Software
- **ESP-IDF 5.2+** installed with `idf.py` in PATH
- **Node.js 18+** and npm
- **Python 3.11+** with pip
- **Docker** (optional, for Unraid API)

---

## Part 1: Home Base Firmware Deployment

### Step 1: Install ESP-IDF

```bash
# Clone ESP-IDF
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
git checkout v5.2

# Install prerequisites
./install.sh esp32p4

# Set up environment (add to .bashrc for persistence)
. ./export.sh
```

### Step 2: Build Firmware

```bash
cd /path/to/e32meshbackend/home_base_firmware

# Configure for ESP32-P4 target
idf.py set-target esp32p4

# Optional: Configure Unraid API URL and other settings
idf.py menuconfig
# Navigate to: Home Base Configuration
# - Set Unraid API URL (default: http://192.168.1.100:8000/logs/ingest)
# - Verify Ethernet PHY settings (IP101, address 1)
# - Verify HTTP server port (default: 80)

# Build the firmware
idf.py build
```

**Expected Output:**
```
[1/5] Performing build step for 'bootloader'
...
[5/5] Generating binary image from built executable
esp-idf v5.2...
Project build complete. To flash, run:
idf.py -p PORT flash
```

### Step 3: Flash to Device

```bash
# Connect P4-ETH-M via Type-C UART cable
# Press and hold BOOT button, press RESET, release BOOT

# Flash firmware (replace /dev/ttyACM0 with your port)
# Windows: COM3, macOS: /dev/cu.usbserial-*
idf.py -p /dev/ttyACM0 flash

# Monitor serial output
idf.py -p /dev/ttyACM0 monitor
```

**Expected Serial Output:**
```
[home_base] === Home Base Firmware Start ===
[device_config] Configuration loaded from NVS
[esp_eth] Ethernet Link Up
[home_base] Ethernet (P4-ETH-M) initialized successfully
[esp_now] ESP-NOW Initialized in STA mode
[http_server] SPIFFS mounted successfully
[http_server] Web server started on port: '80'
[http_server] Web server started with 13 endpoints
[home_base] === Ready for ESP-NOW Mesh Messages ===
```

### Step 4: Verify Firmware

Get the P4's IP address from serial monitor (look for `[esp_eth] Got IP: XXX.XXX.XXX.XXX`).

```bash
# Test status endpoint
curl http://<P4_IP>/api/v1/status

# Expected response:
# {"status":"online","role":"home_base","device_id":"P4-BASE-1","network_id":0,...}

# Test device config portal
curl http://<P4_IP>/
# Should return HTML (device config portal)

# Test device list endpoint
curl http://<P4_IP>/api/v1/devices
# {"devices":[...]}
```

---

## Part 2: Home Base Dashboard Deployment

### Step 1: Build Dashboard

```bash
cd /path/to/e32meshbackend/home_base_dashboard

# Install dependencies
npm install

# Build for production
npm run build

# Verify bundle size
ls -lh dist/
# Should show:
# - index.html (~400 bytes)
# - assets/style-*.css (~13KB)
# - assets/index-*.js (~30KB)
# Total gzipped: ~13.23KB
```

**Build Output:**
```
vite v5.4.21 building for production...
✓ 13 modules transformed.
dist/index.html                  0.39 kB │ gzip:  0.28 kB
dist/assets/style-CG_MyjTw.css  13.26 kB │ gzip:  3.19 kB
dist/assets/index-C4t5LFCR.js   30.19 kB │ gzip: 10.04 kB
✓ built in 783ms
```

### Step 2: Deploy to P4 (Option A: SD Card)

```bash
# Copy built files to P4 SD card
# Assumes SD card mounted on P4 at /mnt/sd/www/
scp -r dist/* root@<P4_IP>:/mnt/sd/www/

# Configure P4 web server to serve from SD card
# (Implementation depends on P4 HTTP server configuration)
```

### Step 3: Deploy to Separate Server (Option B: Development)

```bash
# Run development server on your machine
npm run dev
# Access at http://localhost:3000

# Or serve built files with any static server
cd dist
python -m http.server 3000
# Access at http://localhost:3000
```

### Step 4: Configure API Endpoints

The dashboard expects these endpoints on the P4:
- `GET /api/v1/status` - System status
- `GET /api/v1/devices` - Device list
- `GET /api/logs` - Device logs (with ?device_id= and &limit= params)
- `GET /api/motion` - Motion events
- `POST /api/v1/command` - Send commands to devices
- `WS /ws` - WebSocket for real-time updates

If deploying dashboard separately, update `vite.config.ts` proxy settings:
```typescript
proxy: {
  '/api': {
    target: 'http://<P4_IP>',  // Change this
    changeOrigin: true,
  },
}
```

### Step 5: Verify Dashboard

Open browser to dashboard URL:
1. Should see "Loading Home Base Dashboard..."
2. After 2-3 seconds: System status cards appear
3. Device grid displays connected devices (if any)
4. Click device card to see details
5. Check browser console (F12) for any errors

---

## Part 3: Unraid Central API Deployment

### Step 1: Deploy with Docker

```bash
cd /path/to/e32meshbackend/unraid_api

# Build and start container
docker-compose up -d

# Check logs
docker-compose logs -f

# Expected output:
# INFO:     Application startup complete.
# INFO:     Uvicorn running on http://0.0.0.0:8000
```

### Step 2: Initialize Database

```bash
# Generate admin user and keys (first time only)
docker-compose exec unraid_api python generate_keys.py

# Expected output:
# Admin user created successfully!
# Username: admin
# TOTP Secret: XXXXXXXXXXXXXXXX (add to Google Authenticator)
# Master Signing Key: YYYYYYYY...
```

### Step 3: Verify API

```bash
# Test API health
curl http://localhost:8000/

# Test authentication (requires TOTP code)
curl -X POST http://localhost:8000/auth/session \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","totp_code":"123456"}'

# Should return session token
```

### Step 4: Run Tests

```bash
cd /path/to/e32meshbackend/unraid_api

# Install dependencies
pip install -r requirements.txt
pip install pytest pytest-asyncio

# Run all 23 tests
pytest tests/ -v

# Expected output:
# ================================================= 23 passed in 0.32s =================================================
```

---

## Integration Testing

### End-to-End Test Flow

1. **Start all services:**
   - Unraid API running at `http://localhost:8000`
   - P4 firmware running with Ethernet connection
   - Dashboard accessible at `http://<P4_IP>` or `http://localhost:3000`

2. **Register network in Unraid:**
   ```bash
   # Get session token first
   TOKEN=$(curl -X POST http://localhost:8000/auth/session \
     -H "Content-Type: application/json" \
     -d '{"username":"admin","totp_code":"123456"}' | jq -r '.access_token')
   
   # Create network
   curl -X POST http://localhost:8000/networks \
     -H "Authorization: Bearer $TOKEN" \
     -H "Content-Type: application/json" \
     -d '{"name":"Test Network"}'
   
   # Note the network_id and private_key from response
   ```

3. **Configure P4 with network:**
   ```bash
   # Set device type
   curl -X POST http://<P4_IP>/api/device/set-type \
     -H "Content-Type: application/json" \
     -d '{"type":"motion"}'
   
   # Register with network (use private_key from step 2)
   curl -X POST http://<P4_IP>/api/device/register \
     -H "Content-Type: application/json" \
     -d '{
       "device_id":"ESP32-TEST-001",
       "network_id":1,
       "private_key":"<hex_private_key>"
     }'
   ```

4. **Send test log to verify integration:**
   ```python
   import requests
   import nacl.signing
   import time
   
   # Use the private_key from network registration
   signing_key = nacl.signing.SigningKey(bytes.fromhex('<private_key>'))
   timestamp = int(time.time())
   message = "Test log from device"
   signed = signing_key.sign(f"{timestamp}:{message}".encode())
   
   requests.post("http://localhost:8000/logs/ingest", json={
       "logs": [{
           "device_id": "ESP32-TEST-001",
           "timestamp": timestamp,
           "level": "INFO",
           "category": "test",
           "message": message,
           "signature": signed.signature.hex()
       }]
   })
   ```

5. **Verify in dashboard:**
   - Open dashboard in browser
   - Should see device "ESP32-TEST-001" in device list
   - Click device to see logs
   - Test log should appear in log viewer

---

## Troubleshooting

### Firmware Issues

**Problem: Ethernet not connecting**
```bash
# Check serial monitor for:
# [esp_eth] Ethernet Link Up
# [ip_event] Got IP: XXX.XXX.XXX.XXX

# If not appearing:
1. Verify Ethernet cable is connected and router/switch is powered
2. Check menuconfig: Component config → Ethernet → Enable: YES
3. Check menuconfig: Ethernet PHY model: IP101
4. Try different Ethernet cable
5. Check power supply (5V, sufficient current)
```

**Problem: HTTP endpoints return 404**
```bash
# Check serial output shows:
# [http_server] Web server started with 13 endpoints

# If fewer endpoints:
1. Check http_server.c has all 13 httpd_register_uri_handler() calls
2. Verify config.max_uri_handlers >= 20 in start_webserver()
3. Rebuild firmware: idf.py clean && idf.py build
```

**Problem: SPIFFS not found**
```bash
# If seeing: "SPIFFS not found. Config portal will not be available"
1. Check main/spiffs_image/index.html exists
2. Check CMakeLists.txt has spiffs_create_partition_image() call
3. Rebuild: idf.py build
4. Try: idf.py erase_flash then idf.py flash
```

### Dashboard Issues

**Problem: Dashboard shows blank page**
```bash
# Check browser console (F12) for errors
# Common causes:
1. API endpoint not responding (check P4 is running and accessible)
2. CORS issue (if dashboard on different domain)
3. Incorrect API URL in proxy config

# Test API directly:
curl http://<P4_IP>/api/v1/status
```

**Problem: WebSocket not connecting**
```bash
# Check browser DevTools → Network → WS tab
# Should see: ws://<P4_IP>/ws

# If no connection:
1. Verify firmware has WebSocket support implemented
2. Check browser console for connection errors
3. Test with wscat: wscat -c ws://<P4_IP>/ws
```

**Problem: Device list empty**
```bash
# Check API returns devices:
curl http://<P4_IP>/api/v1/devices

# If empty:
1. No devices are connected via ESP-NOW yet
2. This is normal for fresh installation
3. Will populate once ESP32-S3 devices are deployed
```

### API Issues

**Problem: Tests failing**
```bash
# Common causes:
1. Missing dependencies: pip install -r requirements.txt pytest pytest-asyncio
2. Database locked: rm data/unraid_central.db (in test environment only)
3. Port already in use: Change port in docker-compose.yml

# Debug specific test:
pytest tests/test_logs.py::test_ingest_valid_log -v --tb=short
```

**Problem: Authentication failing**
```bash
# Regenerate admin user:
docker-compose exec unraid_api python generate_keys.py

# Verify TOTP code is current (30-second window)
# Use Google Authenticator or similar app

# Test with curl:
curl -X POST http://localhost:8000/auth/session \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","totp_code":"<current_code>"}'
```

---

## Performance Tuning

### Firmware
- **Memory**: Monitor heap usage in logs. If < 100KB free, reduce log buffer sizes in log_storage.c
- **Network**: Check ESP-NOW RSSI in logs. Should be > -70 dBm for reliable communication
- **HTTP**: Increase `config.max_uri_handlers` if adding more endpoints

### Dashboard
- **Bundle size**: Currently 13.23 KB gzipped, plenty of headroom
- **Refresh rates**: Configured in useAPI.ts (status: 10s, devices: 5s, logs: 30s)
- **WebSocket**: Automatically reconnects on disconnect with exponential backoff

### API
- **Database**: SQLite sufficient for < 10 networks, < 100 devices
- **Logs**: Set retention policy to auto-delete old logs (not yet implemented)
- **Scaling**: For > 100 devices, migrate to PostgreSQL

---

## Security Considerations

### Production Checklist
- [ ] Change default admin password
- [ ] Enable HTTPS for Unraid API (use reverse proxy)
- [ ] Rotate network signing keys periodically
- [ ] Implement log retention policy
- [ ] Set up firewall rules (only allow P4 to access Unraid API)
- [ ] Enable TOTP 2FA for all admin users
- [ ] Review device access logs regularly

### Network Segmentation
Recommended network setup:
```
Internet → Router → Unraid/NAS (192.168.1.100:8000)
                 → P4 Home Base (192.168.1.50:80)
                 → ESP32-S3 Devices (192.168.1.x, ESP-NOW mesh)
```

### Signature Verification
- Firmware currently forwards logs without verification (TODO in http_server.c)
- Unraid API verifies all signatures using Ed25519
- Device private keys shown ONLY ONCE at registration
- Network keys derived from master, device keys derived from network keys

---

## Next Steps

After successful deployment:

1. **Deploy ESP32-S3 device firmware** (see WORKSTREAM_C_FIRMWARE.md)
2. **Build Unraid Central Dashboard** (React UI for multi-network management)
3. **Implement OTA updates** for firmware updates without physical access
4. **Add camera support** for motion-triggered image capture
5. **Set up alerting** for motion events (email, push notifications)

---

## Support

For issues or questions:
1. Check serial monitor output for firmware issues
2. Check browser console (F12) for dashboard issues
3. Check Docker logs for API issues: `docker-compose logs -f`
4. Review documentation:
   - `home_base_firmware/README.md` - Firmware reference
   - `home_base_firmware/QUICKSTART.md` - Quick start guide
   - `PROJECT_STATUS.md` - Overall project status
   - `.github/copilot-instructions.md` - Development guide

---

**Deployment Complete!** 🎉

You now have a fully functional home base system ready for ESP32-S3 device integration.
