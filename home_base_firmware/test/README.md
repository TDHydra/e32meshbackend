# Home Base Firmware Tests

Unit tests for home_base_firmware using ESP-IDF's Unity test framework.

## Building and Running Tests

### Prerequisites
- ESP-IDF 5.2+ with `idf.py` in PATH
- P4-ETH-M board or emulator

### Build Tests
```bash
cd home_base_firmware
idf.py set-target esp32p4
idf.py build
```

### Flash and Run
```bash
idf.py -p COM3 flash monitor
```

The test framework will output results like:
```
Running test_esp_now_mesh.c::test_esp_now_mesh_OnDataRecv_rejects_wrong_message_size
✓ PASS
```

## Test Coverage

### ESP-NOW Mesh Tests (test_esp_now_mesh.c)
- **Message parsing**: Validates mesh_message_t structure handling
- **Type filtering**: Ensures LOG and MOTION are forwarded, HEARTBEAT is local
- **Device tracking**: Preserves device_id and timestamp in queued messages
- **Batch handling**: Tests multiple message types in sequence

**Key scenarios**:
- Invalid message size rejection
- Valid MSG_TYPE_LOG (0x03) acceptance
- Valid MSG_TYPE_MOTION (0x02) acceptance
- Heartbeat (0x01) filtering
- Batch of mixed message types
- Device ID preservation through queue

### HTTP Server Tests (test_http_server.c)
- **Endpoint registration**: Validates /api/v1/status and /api/v1/devices
- **Response format**: Ensures JSON responses are valid
- **Content type**: Verifies application/json header

**Key scenarios**:
- Server starts without error
- GET /api/v1/status returns valid JSON
- GET /api/v1/devices returns JSON array
- Status field indicates "online"

## Test Architecture

Tests follow the **Unity** pattern used throughout ESP-IDF examples:
- Single `.c` file per component (test_*.c)
- `TEST_CASE(description, tag)` macro for each test
- `TEST_ASSERT_*` macros for verification
- Tagged with `[tag]` for filtering (e.g., `[esp_now]`, `[http_server]`)

## Running Specific Tests

Filter by tag:
```bash
idf.py monitor --filter "[esp_now]"
```

Or by name in monitor output - tests run in order.

## Integration Tests (Future)

Once device firmware exists, add integration tests:
1. **Full message round-trip**: Device → ESP-NOW → HTTP → Unraid
2. **Command delivery**: Unraid → Home Base → ESP-NOW → Device
3. **Signature verification**: Invalid signatures rejected end-to-end

Example pattern (from esp-idf/protocols/http_server):
```c
TEST_CASE("end-to-end motion detection", "[integration]") {
    // 1. Simulate device sending motion event
    mesh_message_t motion = {...};
    OnDataRecv(device_mac, (uint8_t*)&motion, sizeof(mesh_message_t));
    
    // 2. Verify HTTP request queued to Unraid
    http_request_t *req = get_pending_request();
    TEST_ASSERT_NOT_NULL(req);
    
    // 3. Verify payload matches expected format
    cJSON *payload = cJSON_Parse(req->body);
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItem(payload, "device_id"));
}
```

## Debugging Tests

Enable ESP_LOG output:
```bash
idf.py set-target esp32p4
idf.py menuconfig
# Go to: Component config → Log output
# Set Default log verbosity to DEBUG
idf.py build
idf.py flash monitor
```

View test details in monitor output. Failures show which TEST_ASSERT failed with expected vs actual values.

## CI/CD Integration

Add to build pipeline:
```bash
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor | grep -E "^(✓|✗|PASS|FAIL)"
```

Expected output pattern:
```
✓ test_esp_now_mesh_OnDataRecv_rejects_wrong_message_size
✓ test_esp_now_mesh_OnDataRecv_accepts_valid_MSG_TYPE_LOG
✓ test_http_server_starts_successfully
```
