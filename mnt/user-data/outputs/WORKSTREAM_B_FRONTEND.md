# WORKSTREAM B: Frontend Development

## Overview
Build three interfaces with different constraints:

| Interface | Hosted On | Stack | Size Limit |
|-----------|-----------|-------|------------|
| Device Config Portal | ESP32 SPIFFS | Vanilla HTML/JS | <100KB |
| Home Base Dashboard | P4 SD Card | Preact + Tailwind | <200KB |
| Unraid Dashboard | Docker | React + shadcn/ui | None |

---

## 1. DEVICE CONFIG PORTAL

**Purpose:** First-time setup wizard when user connects to device AP

### 5-Step Wizard
1. Select device type (Camera / Motion Sensor)
2. WiFi setup (scan + select + password)
3. Network registration (network_id, device_id, private_key from Unraid)
4. Sensor config (PIR sensitivity 1-10, cooldown 5-300s, LED brightness 0-100)
5. Review and save → device reboots

### Device API (provided by firmware)
- GET /api/wifi/scan → network list
- POST /api/config/save → save all settings
- POST /api/reboot → restart

### Deliverables
1. Single HTML/CSS/JS bundle (<100KB gzipped)
2. Mobile-first, works offline

---

## 2. HOME BASE DASHBOARD

**Purpose:** Local network monitoring, no internet required

### Features
- Device grid with online/offline status
- Real-time motion event feed
- Quick controls (LED color, reboot)
- Mesh health stats
- Log viewer

### Consumes Home Base API
- GET /api/v1/status
- GET /api/v1/devices
- POST /api/v1/command
- WS /ws (real-time updates)

### Deliverables
1. Preact SPA (<200KB gzipped)
2. Served from SD card

---

## 3. UNRAID CENTRAL DASHBOARD

**Purpose:** Full admin interface for multi-network management

### Stack
React 18, TypeScript, Vite, Tailwind, shadcn/ui, TanStack Query

### Pages
1. **Login** - TOTP input
2. **Dashboard** - Overview of all networks
3. **Networks** - Create/manage networks
4. **Devices** - Per-network device table with controls
5. **Logs** - Full search/filter/export
6. **Motion Events** - Timeline with media
7. **Firmware** - Upload, deploy, rollback
8. **Settings** - Users, 2FA, VPS failover

### Key UX Rules
- Private key shown ONLY ONCE on device creation (with copy button + warning)
- All admin actions require valid session token
- WebSocket for real-time device status

### Consumes Unraid API
- All endpoints from Workstream A
- Session token in X-Session-Token header

### Deliverables
1. Production build in Docker (nginx)
2. Environment config for API URL

---

## SHARED DESIGN

### Status Colors
- Green: Online / Clear
- Yellow: Degraded / Cooldown
- Red: Offline / Motion detected

### LED States
- Green dot = motion clear
- Yellow dot = cooldown
- Red dot = motion detected

---

## INTEGRATION POINTS

**From Workstream A:**
- OpenAPI spec for all endpoints
- WebSocket message format

**From Workstream C:**
- Device capabilities (camera vs motion)
- Sensor config ranges
