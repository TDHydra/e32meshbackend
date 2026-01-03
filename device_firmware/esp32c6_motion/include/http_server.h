#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "esp_err.h"

/**
 * Start HTTP server for device configuration
 * Serves endpoints matching the device config portal contract:
 * 
 * GET  /api/wifi/scan              - List available WiFi networks
 * POST /api/config/motion           - Update motion sensor config
 * POST /api/config/display          - Update display config
 * POST /api/device/register         - Register with network
 * GET  /api/device/type             - Get current device type
 * GET  /api/v1/status               - Get device status
 * POST /api/reboot                  - Reboot device
 * 
 * Called during AP mode when device is unconfigured
 */
esp_err_t http_server_start(void);

/**
 * Stop HTTP server and cleanup
 */
void http_server_stop(void);

#endif // HTTP_SERVER_H
