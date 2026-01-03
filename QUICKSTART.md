# Quick Start Guide - E32 Mesh Backend

## What Was Done

✅ **Backend API** - Complete with 24 passing tests  
✅ **Device Config Portal** - Production-ready setup wizard  
⏳ **Home Base Dashboard** - Scaffolded, starter component  
📋 **Unraid Dashboard** - Specifications provided  

## Start Development Now

### 1. Device Config Portal (Ready to Deploy)

```bash
# Copy to firmware SPIFFS
cp device_config_portal/index.html home_base_firmware/main/spiffs_image/

# Build firmware with portal
cd home_base_firmware
idf.py build
idf.py -p /dev/ttyACM0 flash monitor

# Implement 3 HTTP endpoints (see FIRMWARE_INTEGRATION.md)
# Then test: connect to ESP32-MESH-{MAC} AP, open http://192.168.4.1
```

### 2. Complete Home Base Dashboard (2-3 hours)

```bash
cd home_base_dashboard
npm install
npm run dev
# Open http://localhost:3000

# Implement missing components:
# 1. src/components/DeviceGrid.tsx
# 2. src/components/MotionFeed.tsx
# 3. src/components/ControlPanel.tsx
# 4. src/components/LogViewer.tsx
# 5. src/hooks/useWebSocket.ts
# 6. src/hooks/useDeviceAPI.ts
# 7. src/types/api.ts

# Then build and deploy
npm run build
npm run preview
scp -r dist/* pi@home-base:/var/www/html/
```

### 3. Build Unraid Dashboard (8-10 hours)

```bash
npm create vite@latest unraid-dashboard -- --template react-ts
cd unraid-dashboard
npm install @shadcn/ui @tanstack/react-query

# Implement 8 pages:
# 1. LoginPage - TOTP authentication
# 2. DashboardPage - Overview
# 3. NetworksPage - Network CRUD
# 4. DevicesPage - Device management
# 5. LogsPage - Log search/filter
# 6. MotionEventsPage - Timeline
# 7. FirmwarePage - OTA updates
# 8. SettingsPage - Users, 2FA

# Follow specs in WORKSTREAM_B_STATUS.md
npm run build
docker build -t unraid-dashboard .
```

### 4. Test Backend API

```bash
cd unraid_api
python -m venv venv
venv\Scripts\activate  # Windows
pip install -r requirements.txt
pytest tests/ -v
# All 24 tests should pass ✅
```

## Key Files to Review

**Architecture & Planning:**
- [.github/copilot-instructions.md](.github/copilot-instructions.md) - Complete architecture (411 lines)
- [PROJECT_STATUS.md](PROJECT_STATUS.md) - Full project overview
- [WORKSTREAM_B_STATUS.md](WORKSTREAM_B_STATUS.md) - Implementation guide

**Implementation Guides:**
- [device_config_portal/FIRMWARE_INTEGRATION.md](device_config_portal/FIRMWARE_INTEGRATION.md) - C code for HTTP handlers
- [home_base_dashboard/README.md](home_base_dashboard/README.md) - Dashboard setup & API specs

**Code:**
- [device_config_portal/index.html](device_config_portal/index.html) - 45KB complete wizard
- [home_base_dashboard/src/App.tsx](home_base_dashboard/src/App.tsx) - Starter component
- [unraid_api/main.py](unraid_api/main.py) - Backend API endpoints

## API Contract Quick Reference

### Device Config Portal (Firmware Must Implement)
```
GET /api/wifi/scan
  Response: [{ ssid: string, rssi: number, security: string }, ...]

POST /api/config/save
  Request: { device_id, network_id, type, private_key, pir_gpio, ... }
  Response: { status: "saved" }

POST /api/reboot
  Response: { status: "rebooting" }
```

### Home Base API (For Local Monitoring)
```
GET /api/v1/status
  Response: { uptime_seconds, device_count, online_count, mesh_rssi, memory_used_mb }

GET /api/v1/devices
  Response: [{ device_id, type, online, rssi, last_seen, motion_state, battery_percent }, ...]

POST /api/v1/command
  Request: { device_id, command, payload }
  Response: { status: "queued", command_id }

WS /ws
  Real-time updates: device_status, motion_event, log_entry
```

### Unraid Central API (For Admin)
```
POST /auth/session
  Request: { username, totp_code }
  Response: { access_token, token_type }

GET/POST /networks
POST /networks/{id}/devices
POST /networks/{id}/command
POST /logs/ingest
GET /logs
```

## Useful Commands

### Run Backend API
```bash
cd unraid_api
python -m venv venv
source venv/bin/activate  # or venv\Scripts\activate (Windows)
pip install -r requirements.txt
python generate_keys.py  # First time setup
uvicorn main:app --reload
```

### Run with Docker
```bash
cd unraid_api
docker-compose up --build
# API will be at http://localhost:8000
```

### Build Device Portal into Firmware
```bash
cp device_config_portal/index.html home_base_firmware/main/spiffs_image/
cd home_base_firmware
idf.py build
idf.py -p COM3 flash monitor  # Adjust port
```

### Build Home Base Dashboard
```bash
cd home_base_dashboard
npm install
npm run build
du -sh dist/  # Check size
```

## Key Decisions Made

✅ **Ed25519** for signing (fast on ESP32: 1-2ms)  
✅ **ESP-NOW** for mesh communication (low latency, 250B packets)  
✅ **Preact** for Home Base Dashboard (lightweight, <50KB)  
✅ **React 18** for Unraid Dashboard (full features)  
✅ **SQLite** for testing, production DB TBD  
✅ **Docker** for Unraid API deployment  
✅ **SPIFFS** for device portal storage  

## What's Ready Now

✅ Copy Device Portal to device firmware  
✅ Implement 3 HTTP endpoints in device firmware  
✅ Deploy to ESP32-S3 devices  
✅ Run backend tests (24/24 passing)  
✅ Test backend API with Swagger UI at `/docs`  

⏳ Complete Home Base Dashboard components  
⏳ Build Unraid Central Dashboard  
⏳ Create device firmware for motion sensors/cameras  

## Troubleshooting

**Device Portal not appearing:**
```
1. Check SPIFFS is built: idf.py build
2. Verify HTTP handler returns index.html
3. Check device AP is active: esp_wifi_set_mode(WIFI_MODE_AP)
4. Monitor device logs: idf.py monitor
```

**Home Base Dashboard API calls failing:**
```
1. Check API is running: curl http://localhost:8000/api/v1/status
2. Verify CORS headers are set
3. Check WebSocket connection: ws.readyState should be 1 (OPEN)
4. Monitor browser console for errors
```

**Backend tests failing:**
```
cd unraid_api/tests
pytest -v --tb=short
# Check SQLite pool configuration in conftest.py
```

## Learning Resources

**If you're new to:**
- **Preact**: Lightweight React, great for embedded (home_base_dashboard/)
- **FastAPI**: Modern Python web framework with automatic docs (/docs endpoint)
- **ESP-IDF**: Espressif IoT Development Framework for ESP32
- **Ed25519**: Elliptic curve cryptography (much faster than RSA)

## Next Steps

**Choose your priority:**

1. **🎯 Complete Home Base Dashboard** (2-3 hours)
   - Highest impact for testing
   - Unblocks deployment on P4
   - Clear requirements in WORKSTREAM_B_STATUS.md

2. **🏢 Build Unraid Dashboard** (8-10 hours)
   - Full admin interface
   - Enables multi-network management
   - More complex but detailed specs provided

3. **📡 Device Firmware** (16-20 hours)
   - Enable actual motion sensors/cameras
   - Requires device hardware to test
   - Specs in WORKSTREAM_C_FIRMWARE.md

**Recommend starting with #1 to get a complete working system end-to-end.**

## Questions?

Refer to documentation:
- Architecture: [.github/copilot-instructions.md](.github/copilot-instructions.md)
- API Details: [WORKSTREAM_A_BACKEND.md](WORKSTREAM_A_BACKEND.md)
- Frontend Specs: [WORKSTREAM_B_FRONTEND.md](WORKSTREAM_B_FRONTEND.md)
- Implementation: [WORKSTREAM_B_STATUS.md](WORKSTREAM_B_STATUS.md)

---

**Current Status: 50% Complete - Ready to Continue!**

