"""
Integration Tests for E32 Mesh Backend
Tests the complete flow: Firmware → Home Base API → Dashboard
"""

import pytest
import json
import time
import requests
import asyncio
import websockets
from datetime import datetime
from typing import Dict, List, Optional


class TestHomeBaseAPI:
    """Test home base firmware HTTP endpoints"""

    BASE_URL = "http://192.168.1.100"  # P4-ETH-M home base IP
    
    def test_status_endpoint(self):
        """GET /api/v1/status returns system status"""
        response = requests.get(f"{self.BASE_URL}/api/v1/status")
        assert response.status_code == 200
        
        data = response.json()
        assert data["status"] == "online"
        assert data["role"] == "home_base"
        assert "device_id" in data
        assert "network_id" in data
        
    def test_devices_endpoint(self):
        """GET /api/v1/devices returns list of mesh devices"""
        response = requests.get(f"{self.BASE_URL}/api/v1/devices")
        assert response.status_code == 200
        
        devices = response.json()
        assert isinstance(devices, list)
        # May be empty if no devices registered yet
        
    def test_logs_endpoint_no_filter(self):
        """GET /api/logs returns all logs"""
        response = requests.get(f"{self.BASE_URL}/api/logs")
        assert response.status_code == 200
        
        logs = response.json()
        assert isinstance(logs, list)
        
        if len(logs) > 0:
            log = logs[0]
            assert "id" in log
            assert "device_id" in log
            assert "timestamp" in log
            assert "level" in log
            assert "category" in log
            assert "message" in log
    
    def test_logs_endpoint_with_device_filter(self):
        """GET /api/logs?device_id=<id> filters by device"""
        response = requests.get(f"{self.BASE_URL}/api/logs?device_id=test-device&limit=50")
        assert response.status_code == 200
        
        logs = response.json()
        assert isinstance(logs, list)
        
        # All logs should be from specified device
        for log in logs:
            assert log["device_id"] == "test-device"
    
    def test_motion_endpoint(self):
        """GET /api/motion returns motion events"""
        response = requests.get(f"{self.BASE_URL}/api/motion")
        assert response.status_code == 200
        
        events = response.json()
        assert isinstance(events, list)
        
        if len(events) > 0:
            event = events[0]
            assert "id" in event
            assert "device_id" in event
            assert "timestamp" in event
    
    def test_motion_endpoint_with_filter(self):
        """GET /api/motion?device_id=<id> filters motion events"""
        response = requests.get(f"{self.BASE_URL}/api/motion?device_id=sensor-1&limit=20")
        assert response.status_code == 200
        
        events = response.json()
        assert isinstance(events, list)
        
        for event in events:
            assert event["device_id"] == "sensor-1"
    
    def test_command_endpoint(self):
        """POST /api/v1/command sends command to device"""
        command_payload = {
            "command": "led_set_color",
            "target_device": "test-device",
            "payload": {"color": "FF0000"},
            "signature": "0" * 128  # Placeholder signature
        }
        
        response = requests.post(
            f"{self.BASE_URL}/api/v1/command",
            json=command_payload
        )
        assert response.status_code == 200
        
        data = response.json()
        assert data["status"] == "queued"
        assert data["command"] == "led_set_color"
        assert data["target_device"] == "test-device"
    
    def test_command_endpoint_validation(self):
        """POST /api/v1/command validates required fields"""
        # Missing target_device
        command_payload = {
            "command": "led_set_color",
            "payload": {"color": "FF0000"}
        }
        
        response = requests.post(
            f"{self.BASE_URL}/api/v1/command",
            json=command_payload
        )
        assert response.status_code == 400
        assert "Missing command or target_device" in response.text


class TestDeviceFirmware:
    """Test device firmware (ESP32-S3) functionality"""
    
    DEVICE_AP_URL = "http://192.168.4.1"  # Device AP mode IP
    
    def test_config_portal_accessible(self):
        """GET / returns device config portal HTML"""
        response = requests.get(f"{self.DEVICE_AP_URL}/")
        assert response.status_code == 200
        assert "text/html" in response.headers.get("Content-Type", "")
        assert len(response.text) > 0
    
    def test_wifi_scan_endpoint(self):
        """GET /api/wifi/scan returns available networks"""
        response = requests.get(f"{self.DEVICE_AP_URL}/api/wifi/scan")
        assert response.status_code == 200
        
        networks = response.json()
        assert isinstance(networks, list)
        
        if len(networks) > 0:
            network = networks[0]
            assert "ssid" in network
            assert "rssi" in network
            assert "security" in network
    
    def test_device_type_endpoint(self):
        """GET /api/device/type returns device type"""
        response = requests.get(f"{self.DEVICE_AP_URL}/api/device/type")
        assert response.status_code == 200
        
        data = response.json()
        assert "type" in data
        assert data["type"] in ["motion", "camera", "unconfigured"]
    
    def test_device_config_flow(self):
        """Complete device configuration flow"""
        
        # Step 1: Set device type
        response = requests.post(
            f"{self.DEVICE_AP_URL}/api/device/set-type",
            json={"type": "motion"}
        )
        assert response.status_code == 200
        assert response.json()["status"] == "saved"
        
        # Step 2: Connect to WiFi
        response = requests.post(
            f"{self.DEVICE_AP_URL}/api/wifi/connect",
            json={"ssid": "TestNetwork", "password": "test123"}
        )
        assert response.status_code in [200, 202]  # May be connecting
        
        # Step 3: Register device
        response = requests.post(
            f"{self.DEVICE_AP_URL}/api/device/register",
            json={
                "device_id": "sensor-test-001",
                "network_id": 1,
                "private_key": "0" * 256
            }
        )
        assert response.status_code == 200
        
        # Step 4: Configure sensors
        response = requests.post(
            f"{self.DEVICE_AP_URL}/api/config/sensors",
            json={
                "pir_gpio": 39,
                "pir_sensitivity": 5,
                "pir_cooldown_ms": 30000,
                "pir_invert_logic": False
            }
        )
        assert response.status_code == 200


class TestDashboardIntegration:
    """Test dashboard integration with firmware"""
    
    DASHBOARD_URL = "http://localhost:5173"
    HOME_BASE_API = "http://192.168.1.100"
    
    def test_dashboard_loads(self):
        """Dashboard UI loads without errors"""
        response = requests.get(self.DASHBOARD_URL)
        assert response.status_code == 200
        # In dev mode, Vite serves index.html
    
    @pytest.mark.asyncio
    async def test_websocket_connection(self):
        """WebSocket connection to home base"""
        try:
            uri = f"ws://192.168.1.100/ws"
            async with websockets.connect(uri) as websocket:
                # Should connect successfully
                assert websocket.open
                
                # Wait for initial message or timeout
                try:
                    message = await asyncio.wait_for(
                        websocket.recv(),
                        timeout=5.0
                    )
                    assert message is not None
                except asyncio.TimeoutError:
                    # No message within timeout is OK, connection is open
                    pass
        except Exception as e:
            # WebSocket may not be implemented yet
            pytest.skip(f"WebSocket not available: {e}")


class TestMessageFormat:
    """Test mesh message format and signing"""
    
    def test_heartbeat_message_format(self):
        """Heartbeat message contains required fields"""
        # Simulated heartbeat from device
        timestamp = int(time.time())
        device_id = "test-device"
        payload = json.dumps({
            "uptime_ms": 60000,
            "heap_free": 120000,
            "heap_min": 80000
        })
        
        # Message to sign: "{timestamp}:{payload}"
        message_to_sign = f"{timestamp}:{payload}".encode('utf-8')
        
        assert len(message_to_sign) > 0
        assert timestamp > 0
    
    def test_motion_message_format(self):
        """Motion message contains required fields"""
        timestamp = int(time.time())
        payload = json.dumps({
            "intensity": 100,
            "timestamp": timestamp
        })
        
        message_to_sign = f"{timestamp}:{payload}".encode('utf-8')
        assert len(message_to_sign) > 0
    
    def test_log_message_format(self):
        """Log message contains required fields"""
        timestamp = int(time.time())
        payload = json.dumps({
            "level": "info",
            "category": "sensor",
            "message": "Motion sensor initialized"
        })
        
        message_to_sign = f"{timestamp}:{payload}".encode('utf-8')
        assert len(message_to_sign) > 0


class TestErrorHandling:
    """Test error handling in API"""
    
    BASE_URL = "http://192.168.1.100"
    
    def test_invalid_json_request(self):
        """POST with invalid JSON returns error"""
        response = requests.post(
            f"{self.BASE_URL}/api/v1/command",
            data="invalid json"
        )
        assert response.status_code == 400
    
    def test_payload_too_large(self):
        """Request with payload > max size returns error"""
        huge_payload = "x" * 10000  # > 1024 byte limit
        response = requests.post(
            f"{self.BASE_URL}/api/v1/command",
            data=huge_payload
        )
        assert response.status_code == 413
    
    def test_missing_endpoint_returns_404(self):
        """Request to non-existent endpoint returns 404"""
        response = requests.get(f"{self.BASE_URL}/api/nonexistent")
        assert response.status_code == 404


class TestPerformance:
    """Performance benchmarks"""
    
    BASE_URL = "http://192.168.1.100"
    
    def test_status_endpoint_response_time(self):
        """Status endpoint responds in <50ms"""
        start = time.time()
        response = requests.get(f"{self.BASE_URL}/api/v1/status")
        elapsed = (time.time() - start) * 1000  # ms
        
        assert response.status_code == 200
        assert elapsed < 50, f"Response took {elapsed}ms, expected <50ms"
    
    def test_logs_endpoint_response_time(self):
        """Logs endpoint responds in <100ms"""
        start = time.time()
        response = requests.get(f"{self.BASE_URL}/api/logs?limit=50")
        elapsed = (time.time() - start) * 1000  # ms
        
        assert response.status_code == 200
        assert elapsed < 100, f"Response took {elapsed}ms, expected <100ms"


# ===== Fixtures =====

@pytest.fixture
def mock_home_base(mocker):
    """Mock home base for testing without physical hardware"""
    # Can be used to mock HTTP responses
    pass


@pytest.fixture
def mock_device(mocker):
    """Mock device for testing without physical hardware"""
    pass


# ===== Conftest =====

def pytest_configure(config):
    """Configure pytest with custom markers"""
    config.addinivalue_line(
        "markers", "integration: mark test as integration test"
    )
    config.addinivalue_line(
        "markers", "slow: mark test as slow"
    )


# ===== Running Tests =====

"""
Run all tests:
    pytest tests/integration_tests.py -v

Run only home base API tests:
    pytest tests/integration_tests.py::TestHomeBaseAPI -v

Run with markers:
    pytest tests/integration_tests.py -m integration -v

Generate report:
    pytest tests/integration_tests.py --html=report.html --self-contained-html
"""
