# Device Config Portal

A lightweight, zero-dependency device setup wizard for ESP32 mesh network devices.

## Features

✅ **Mobile-first design** - Works perfectly on phones and tablets  
✅ **Offline capable** - All logic runs client-side  
✅ **Size optimized** - Single HTML file, ~45KB (< 100KB requirement)  
✅ **5-step wizard**:
1. Select device type (Motion Sensor / Camera)
2. Configure WiFi network
3. Register with Unraid network
4. Set sensor parameters (PIR sensitivity, cooldown, LED brightness)
5. Review and save

✅ **Real-time validation** - Clear error messages for invalid inputs  
✅ **No external dependencies** - Pure vanilla HTML/CSS/JavaScript

## Installation

### For Device SPIFFS (First-Time Setup)

1. **Copy to SPIFFS partition:**
```bash
# In home_base_firmware/main/spiffs directory
cp ../../../device_config_portal/index.html spiffs_image/
```

2. **Build SPIFFS image:**
```bash
cd home_base_firmware
idf.py build
# SPIFFS image is generated automatically
```

3. **Flash device:**
```bash
idf.py -p /dev/ttyACM0 flash
```

### For Local Testing

Simply open `index.html` in a browser (works offline, connect to device AP to test API calls).

## API Endpoints (Device Firmware)

The portal expects these endpoints from the device's HTTP server:

### GET /api/wifi/scan
Scan for available WiFi networks.

**Response:**
```json
[
  {
    "ssid": "MyNetwork",
    "rssi": -45,
    "security": "WPA2"
  },
  {
    "ssid": "GuestNetwork", 
    "rssi": -68,
    "security": "open"
  }
]
```

### POST /api/config/save
Save device configuration and trigger reboot.

**Request:**
```json
{
  "device_id": "ESP32-ABC123",
  "network_id": 1,
  "type": "motion",
  "private_key": "hex_encoded_key",
  "pir_gpio": 39,
  "pir_sensitivity": 5,
  "pir_cooldown_ms": 30000,
  "led_gpio": 48,
  "led_brightness": 80,
  "board_variant": "esp32s3_devkitm"
}
```

**Response:**
```json
{
  "status": "saved"
}
```

### POST /api/reboot
Reboot the device.

**Response:**
```json
{
  "status": "rebooting"
}
```

## User Flow

### First-Time Setup (Device AP Mode)

1. User connects to device's AP: `ESP32-MESH-{MAC}`
2. Browser redirects to `192.168.4.1`
3. Device Config Portal loads from SPIFFS
4. User completes 5-step wizard
5. Configuration saved to NVS, device reboots
6. Device connects to configured WiFi
7. User can now manage device via Home Base Dashboard or Unraid Central

### Configuration Storage (NVS)

Device stores configuration under `device_config` key:
```json
{
  "device_id": "ESP32-ABC123",
  "network_id": 1,
  "type": "motion",
  "private_key": "hex_key",
  "pir_gpio": 39,
  "pir_sensitivity": 5,
  "pir_cooldown_ms": 30000,
  "led_gpio": 48,
  "led_brightness": 80,
  "board_variant": "esp32s3_devkitm",
  "wifi_ssid": "MyNetwork",
  "wifi_password": "encrypted"
}
```

## Browser Compatibility

- Chrome/Chromium 60+
- Firefox 55+
- Safari 12+
- Edge 79+
- Mobile browsers (iOS Safari, Chrome Mobile)

## Customization

### Change Colors
Edit CSS variables in `<style>` section:
```css
:root {
    --color-primary: #3b82f6;      /* Primary blue */
    --color-success: #10b981;      /* Success green */
    --color-warning: #f59e0b;      /* Warning orange */
    --color-danger: #ef4444;       /* Error red */
}
```

### Add Custom Steps
Add new `<div class="step">` blocks and increment `totalSteps`:
```javascript
const totalSteps = 6; // Was 5
```

### Adjust Form Defaults
Modify the `config` object initial values:
```javascript
const config = {
    pirGpio: 39,      // Change default PIR pin
    ledGpio: 48,      // Change default LED pin
    // ... etc
}
```

## Size Analysis

Current implementation:
- HTML + CSS + JS: ~45KB
- Minified: ~28KB
- Gzipped: ~10KB

**Well under 100KB limit** ✅

## Accessibility

- Semantic HTML (form elements, labels)
- ARIA-friendly radio inputs
- Color contrast meets WCAG AA standards
- Keyboard navigation support
- Mobile touch-friendly input sizes

## Security Notes

⚠️ **Private Key Display**
- Private key shown in plaintext (device-local only)
- Should only be used in secure, local setup mode
- Consider adding local encryption for production
- NVS storage should use encryption (ESP-IDF menuconfig)

## Testing

### Manual Testing Checklist
- [ ] Mobile viewport (320px, 768px, 1024px)
- [ ] WiFi scan with 0, 5, 10+ networks
- [ ] Invalid form submissions (missing fields)
- [ ] Back/Next navigation through all steps
- [ ] Summary accuracy on final step
- [ ] Save triggers reboot endpoint
- [ ] Success screen displays after save

### API Mock (for offline testing)
```javascript
// In browser console, add mock responses:
window.fetch = (url, opts) => {
    if (url.includes('wifi/scan')) {
        return Promise.resolve({
            ok: true,
            json: () => Promise.resolve([
                { ssid: 'TestNetwork', rssi: -45, security: 'WPA2' }
            ])
        });
    }
    // ... more mocks
};
```

## Deployment Checklist

Before deploying to production devices:
- [ ] Test on all target device boards (DevKitM, Freenove, XIAO)
- [ ] Verify API endpoints match firmware implementation
- [ ] Check NVS config storage and retrieval
- [ ] Test reboot after configuration
- [ ] Verify WiFi reconnection works
- [ ] Test with various WiFi password complexities
- [ ] Check error handling for lost connection
- [ ] Validate on low-bandwidth networks (test RSSI -80dBm)

## Firmware Integration Example

```c
// In firmware HTTP server setup:
httpd_uri_t portal_get_handler = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = http_get_handler,
    .user_ctx = NULL
};
httpd_register_uri_handler(server, &portal_get_handler);

// Handler serves index.html from SPIFFS
static esp_err_t http_get_handler(httpd_req_t *req) {
    FILE *f = fopen("/spiffs/index.html", "r");
    // ... read and send file
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return ESP_OK;
}
```

## Future Enhancements

- 🔄 Add WiFi password strength validator
- 🔒 Implement HTTPS with self-signed cert
- 📊 Add network signal quality indicator
- 🌙 Dark mode toggle
- 🌍 Multi-language support (i18n)
- 💾 Configuration export/import
- 🔐 QR code for private key sharing
- 📱 Progressive Web App (PWA) offline cache

## License

Part of E32 Mesh Backend project. See parent repository for license.

## Support

For issues or questions:
1. Check device logs: `idf.py monitor`
2. Verify WiFi scan returns networks: `curl http://192.168.4.1/api/wifi/scan`
3. Check NVS storage: Use `nvs_dump` partition tool
4. Review firmware HTTP server configuration in home_base_firmware/main/http_server.c

