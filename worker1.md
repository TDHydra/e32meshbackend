# WORKSTREAM A: Backend Development

## Overview
Build two APIs: Unraid Central (Python/FastAPI in Docker) + Home Base (C on ESP32-P4-ETH-M)

---

## UNRAID CENTRAL API

**Stack:** Python 3.11, FastAPI, SQLAlchemy, Redis, Docker

### Database Tables
- networks (id, name, home_base_ip, public_key)
- devices (id, network_id, type, public_key, config, online, last_seen)
- device_logs (device_id, timestamp, level, category, message)
- motion_events (device_id, timestamp, media_path)
- firmware (version, chip, type, hash, signature, path)
- users (username, totp_secret, verified)

### Security
- **Ed25519** for signing (PyNaCl) - derive network keys from master, device keys from network
- **TOTP** for 2FA (pyotp) - Google Authenticator compatible
- **Session tokens** - 15 min expiry, required for all admin endpoints
- **Command signing** - timestamp + nonce to prevent replay

### API Endpoints
| Endpoint | Purpose |
|----------|---------|
| POST /auth/session | Login with TOTP → session token |
| GET/POST /networks | List/create networks |
| GET/POST /networks/{id}/devices | List/register devices (returns private key ONCE) |
| POST /networks/{id}/command | Send signed command to device |
| POST /logs/ingest | Receive log batch from home base |
| GET /logs | Query with filters |
| POST /firmware | Upload signed firmware |
| POST /firmware/deploy | Push OTA to devices |
| WS /ws | Real-time status stream |

### Deliverables
1. Docker container with docker-compose.yml
2. OpenAPI spec for frontend team
3. Key generation script for initial setup

---

## HOME BASE API (P4-ETH-M)

**Stack:** ESP-IDF 5.2+, esp_http_server, cJSON

### Hardware Config
- Ethernet: IP101 PHY (built-in, RMII interface)
- SD Card: FAT32 for logs/media
- OLED: SSD1306 on I2C (GPIO6/7)
- C6 Coprocessor: **DISABLE** (use Ethernet only for stability)

### Responsibilities
- ESP-NOW mesh coordinator (receive from devices)
- Local HTTP API for dashboard
- Forward logs to Unraid (batch every 60s)
- Verify and relay signed commands
- Store media to SD card

### API Endpoints
| Endpoint | Purpose |
|----------|---------|
| GET /api/v1/status | System status |
| GET /api/v1/devices | List mesh devices |
| POST /api/v1/command | Receive signed command from Unraid |
| POST /api/v1/logs | Receive logs from mesh devices |
| WS /ws | Real-time updates to local dashboard |

### Deliverables
1. Firmware binary for P4-ETH-M
2. SD card filesystem layout spec

---

## SHARED PROTOCOL

### ESP-NOW Message Format
| Field | Size | Description |
|-------|------|-------------|
| type | 1B | 0x01=heartbeat, 0x02=motion, 0x03=log, 0x04=command |
| device_id | 16B | Null-terminated string |
| timestamp | 4B | Unix timestamp |
| payload | 200B | JSON data |
| signature | 64B | Ed25519 signature |

### Integration Points
- Provide OpenAPI spec to Workstream B
- Define message format for Workstream C
- WebSocket events: device_status, motion_event, log_entry