# Home Base - Quick Start (5 Minutes)

Everything is ready! Here's how to get running immediately.

## Step 1: Build Firmware (2 min)

```bash
cd home_base_firmware
idf.py set-target esp32p4
idf.py build
```

## Step 2: Flash to P4-ETH-M (1 min)

1. Plug in P4-ETH-M with USB Type-C cable to computer
2. Hold BOOT button, press RESET, release BOOT
3. Run:
```bash
idf.py -p COM3 flash monitor
# (On Mac: /dev/cu.usbserial-*, Linux: /dev/ttyACM0)
```

Watch for output like:
```
[home_base] === Home Base Firmware Start ===
[esp_eth] Ethernet Link Up
[http_server] Web server started with 13 endpoints
[home_base] === Ready for ESP-NOW Mesh Messages ===
```

## Step 3: Start Dashboard (2 min)

In another terminal:

```bash
cd home_base_dashboard
npm install
npm run dev
```

Visit: `http://localhost:5173`

---

## Test Checklist

**Firmware:**
- [ ] Ethernet link up in serial monitor
- [ ] HTTP server started with 13 endpoints

**Dashboard:**
- [ ] Page loads at localhost:5173
- [ ] System status cards appear
- [ ] "No devices connected" message (expected - no S3 devices yet)

**Integration:**
```bash
# From any terminal, test endpoints:
curl http://<P4_IP>/api/v1/status
# Should return: {"status":"online","role":"home_base",...}

curl http://<P4_IP>/
# Should return HTML (config portal)
```

---

## Find Your P4-ETH-M IP

**Option 1: Check router**
- Log into your WiFi router admin page
- Look for "P4-ETH-M" or similar hostname
- Note the IP address (e.g., 192.168.1.100)

**Option 2: Scan network**
```bash
# Linux/Mac:
arp -a | grep -i p4

# Or use a network scanner like:
# - Angry IP Scanner
# - Network Utility
# - nmap -sn 192.168.1.0/24
```

**Option 3: Serial monitor**
Watch the serial monitor output - the IP should be printed when Ethernet connects.

---

## Troubleshooting

**Ethernet not connecting:**
- Check physical cable (must be plugged in Type-C port, not USB)
- Power cycle device (unplug 10 seconds, replug)
- Check firewall isn't blocking port 80

**Dashboard loads but no API response:**
- Verify P4 IP is correct
- Test in browser: `http://<IP>/api/v1/status`
- Check P4 is on same network as your computer

**npm install fails:**
- Ensure Node.js 18+ installed: `node --version`
- Delete node_modules: `rm -rf node_modules package-lock.json`
- Run `npm install` again

---

## Next: Add Your First Device

Once dashboard works, you can:

1. **Unconfigure P4**: Connect to `ESP32-MESH-XXXX` WiFi AP
2. **Visit setup wizard**: `http://192.168.4.1`
3. **Configure new motion sensor**: See WORKSTREAM_C_FIRMWARE.md

---

## Files Modified

Firmware:
- ✅ main.c (ethernet config)
- ✅ http_server.c (SPIFFS serving)
- ✅ CMakeLists.txt (added spiffs partition)

Dashboard:
- ✅ src/App.tsx (complete rewrite)
- ✅ src/hooks/useAPI.ts (new hooks)
- ✅ src/components/*.tsx (4 new components)

Ready to roll! 🚀
