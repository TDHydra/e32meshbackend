# Integration Testing Guide

## Overview

This document describes how to test the complete E32 Mesh Backend system, including:
- Home base firmware endpoints (HTTP + WebSocket)
- Device firmware (ESP32-S3) configuration flow
- Dashboard integration
- Message format validation
- Performance benchmarks

## Prerequisites

### Hardware
- **Home Base**: ESP32-P4-ETH-M with Ethernet connection (IP: 192.168.1.100)
- **Device**: ESP32-S3 with WiFi (AP mode at 192.168.4.1 when unconfigured)
- **Network**: LAN connectivity between test machine and devices

### Software
```bash
pip install pytest pytest-asyncio websockets requests
```

### Configuration
Edit `tests/integration_tests.py` to match your network IPs:
```python
BASE_URL = "http://192.168.1.100"       # Home base IP
DEVICE_AP_URL = "http://192.168.4.1"    # Device AP IP
DASHBOARD_URL = "http://localhost:5173" # Dashboard dev server
```

## Running Tests

### All Tests
```bash
cd /path/to/e32meshbackend
pytest tests/integration_tests.py -v
```

### Specific Test Classes
```bash
# Test only home base API
pytest tests/integration_tests.py::TestHomeBaseAPI -v

# Test only device firmware
pytest tests/integration_tests.py::TestDeviceFirmware -v

# Test only dashboard integration
pytest tests/integration_tests.py::TestDashboardIntegration -v

# Test message formats
pytest tests/integration_tests.py::TestMessageFormat -v

# Test error handling
pytest tests/integration_tests.py::TestErrorHandling -v

# Test performance
pytest tests/integration_tests.py::TestPerformance -v
```

### By Marker
```bash
# Integration tests only
pytest tests/integration_tests.py -m integration -v

# Skip slow tests
pytest tests/integration_tests.py -m "not slow" -v
```

### With Output
```bash
# Show print statements
pytest tests/integration_tests.py -v -s

# Generate HTML report
pytest tests/integration_tests.py --html=report.html --self-contained-html

# Detailed failure output
pytest tests/integration_tests.py -v --tb=long
```

## Test Scenarios

### Scenario 1: Home Base API Verification

**Goal**: Verify all firmware endpoints are working correctly

**Steps**:
1. Power on P4-ETH-M with Ethernet connected
2. Wait for Ethernet link (check serial: "Ethernet Link Up")
3. Run tests:
   ```bash
   pytest tests/integration_tests.py::TestHomeBaseAPI -v
   ```

**Expected Results**:
- ✅ Status endpoint returns online status
- ✅ Devices endpoint returns device list (may be empty)
- ✅ Logs endpoint returns log entries
- ✅ Motion endpoint returns motion events
- ✅ Command endpoint accepts POST requests

### Scenario 2: Device Configuration Flow

**Goal**: Verify device can be configured via AP mode

**Steps**:
1. Power on ESP32-S3 device
2. Device should appear as "ESP32-MESH-XXXXXX" WiFi network
3. Connect from test machine to that AP
4. Run tests:
   ```bash
   pytest tests/integration_tests.py::TestDeviceFirmware -v
   ```

**Expected Results**:
- ✅ Config portal loads at 192.168.4.1
- ✅ WiFi scan endpoint returns available networks
- ✅ Device type endpoint shows current type
- ✅ Complete configuration flow succeeds (type → WiFi → register → sensors)

### Scenario 3: Dashboard Integration

**Goal**: Verify dashboard connects to firmware and receives real-time updates

**Steps**:
1. Start home base firmware on P4-ETH-M
2. Start dashboard dev server:
   ```bash
   cd home_base_dashboard
   npm run dev
   ```
3. Open http://localhost:5173 in browser
4. Run tests:
   ```bash
   pytest tests/integration_tests.py::TestDashboardIntegration -v
   ```

**Expected Results**:
- ✅ Dashboard UI loads
- ✅ API requests to home base succeed
- ✅ WebSocket connects (or gracefully fails if not implemented)

### Scenario 4: Message Format Validation

**Goal**: Verify message format matches specification

**Steps**:
```bash
pytest tests/integration_tests.py::TestMessageFormat -v
```

**Expected Results**:
- ✅ Heartbeat includes uptime, heap stats
- ✅ Motion event includes intensity, timestamp
- ✅ Log message includes level, category, message
- ✅ All messages use "{timestamp}:{payload}" signing format

### Scenario 5: Error Handling

**Goal**: Verify proper error responses

**Steps**:
```bash
pytest tests/integration_tests.py::TestErrorHandling -v
```

**Expected Results**:
- ✅ Invalid JSON returns 400
- ✅ Oversized payload returns 413
- ✅ Missing endpoint returns 404
- ✅ Missing required fields return 400

### Scenario 6: Performance Benchmarks

**Goal**: Verify endpoints meet performance targets

**Steps**:
```bash
pytest tests/integration_tests.py::TestPerformance -v
```

**Expected Results**:
- ✅ Status endpoint responds in <50ms
- ✅ Logs endpoint responds in <100ms
- ✅ WebSocket latency <100ms

## Manual Testing

### Testing Status Endpoint
```bash
curl -X GET http://192.168.1.100/api/v1/status | jq
```

**Expected Output**:
```json
{
  "status": "online",
  "role": "home_base",
  "device_id": "P4-ETH-M-001",
  "network_id": 1
}
```

### Testing Logs Endpoint
```bash
curl -X GET "http://192.168.1.100/api/logs?limit=10" | jq
```

**Expected Output**:
```json
[
  {
    "id": 1,
    "device_id": "sensor-001",
    "timestamp": 1704307200,
    "level": "info",
    "category": "system",
    "message": "Startup complete"
  }
]
```

### Testing Command Endpoint
```bash
curl -X POST http://192.168.1.100/api/v1/command \
  -H "Content-Type: application/json" \
  -d '{
    "command": "led_set_color",
    "target_device": "sensor-001",
    "signature": "0000000000000000"
  }' | jq
```

### Testing Device Config Portal
```bash
# Check if portal is served
curl -X GET http://192.168.4.1/ | head -50

# Check WiFi scan
curl -X GET http://192.168.4.1/api/wifi/scan | jq
```

## Debugging Test Failures

### HTTP 404 on Endpoint
**Problem**: `requests.exceptions.ConnectionError`
**Solution**: 
- Verify device IP in test file
- Check device has power and network connectivity
- Verify endpoint is registered in http_server.c

### Test Timeout
**Problem**: `requests.Timeout`
**Solution**:
- Check network connectivity
- Monitor serial output for errors
- Verify firmware was built with HTTP server enabled

### WebSocket Connection Refused
**Problem**: `ConnectionRefusedError`
**Solution**:
- WebSocket `/ws` endpoint not yet implemented
- This is expected in current version
- Mark test with `@pytest.mark.skip()` or `pytest.skip()`

### JSON Parsing Error
**Problem**: `json.JSONDecodeError`
**Solution**:
- Check Content-Type header is `application/json`
- Log response.text to see actual response
- Verify cJSON is building valid output

## Continuous Integration

### GitHub Actions Example
```yaml
name: Integration Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - uses: actions/setup-python@v2
        with:
          python-version: '3.10'
      - name: Install dependencies
        run: pip install -r tests/requirements.txt
      - name: Run integration tests
        run: pytest tests/integration_tests.py -v
        env:
          HOME_BASE_IP: 192.168.1.100
          DEVICE_AP_IP: 192.168.4.1
```

## Expected Test Results

### All Tests Pass
```
collected 25 items

TestHomeBaseAPI::test_status_endpoint PASSED                      [  4%]
TestHomeBaseAPI::test_devices_endpoint PASSED                     [  8%]
TestHomeBaseAPI::test_logs_endpoint_no_filter PASSED              [ 12%]
TestHomeBaseAPI::test_logs_endpoint_with_device_filter PASSED     [ 16%]
TestHomeBaseAPI::test_motion_endpoint PASSED                      [ 20%]
TestHomeBaseAPI::test_motion_endpoint_with_filter PASSED          [ 24%]
TestHomeBaseAPI::test_command_endpoint PASSED                     [ 28%]
TestHomeBaseAPI::test_command_endpoint_validation PASSED          [ 32%]
TestDeviceFirmware::test_config_portal_accessible PASSED          [ 36%]
TestDeviceFirmware::test_wifi_scan_endpoint PASSED                [ 40%]
TestDeviceFirmware::test_device_type_endpoint PASSED              [ 44%]
TestDeviceFirmware::test_device_config_flow PASSED                [ 48%]
TestDashboardIntegration::test_dashboard_loads PASSED             [ 52%]
TestDashboardIntegration::test_websocket_connection SKIPPED       [ 56%]
TestMessageFormat::test_heartbeat_message_format PASSED           [ 60%]
TestMessageFormat::test_motion_message_format PASSED              [ 64%]
TestMessageFormat::test_log_message_format PASSED                 [ 68%]
TestErrorHandling::test_invalid_json_request PASSED               [ 72%]
TestErrorHandling::test_payload_too_large PASSED                  [ 76%]
TestErrorHandling::test_missing_endpoint_returns_404 PASSED       [ 80%]
TestPerformance::test_status_endpoint_response_time PASSED        [ 84%]
TestPerformance::test_logs_endpoint_response_time PASSED          [ 88%]

======================== 21 passed, 1 skipped in 3.45s ========================
```

## Coverage

To measure code coverage:
```bash
pytest tests/integration_tests.py --cov=unraid_api --cov=home_base_firmware --cov-report=html
open htmlcov/index.html
```

## Next Steps

1. **Implement WebSocket**: Add `/ws` endpoint to http_server.c for real-time updates
2. **Add Message Signing**: Implement Ed25519 signature verification in command handler
3. **Implement Device Mesh**: Complete ESP-NOW message routing between devices
4. **Add OTA Updates**: Implement firmware update delivery to devices
5. **Add Persistent Storage**: Store logs and motion events to NVS for recovery

## References

- [Protocol Specification](../home_base_firmware/main/include/protocol.h)
- [HTTP Server Implementation](../home_base_firmware/main/http_server.c)
- [Device Firmware](../device_firmware/components/device_firmware/device_firmware.c)
- [Dashboard](../home_base_dashboard/src/App.tsx)
