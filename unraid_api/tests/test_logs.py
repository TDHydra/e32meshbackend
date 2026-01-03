"""API tests for log ingestion and signature verification."""

import pytest
import security


def test_ingest_valid_log(client, test_device, test_keypair, db_session):
    """Test ingesting a log with valid signature."""
    timestamp = 1704268800
    message = "Motion detected in livingroom"
    
    # Create valid signature
    message_to_sign = f"{int(timestamp)}:{message}".encode('utf-8')
    signed = test_keypair["signing_key"].sign(message_to_sign)
    signature_hex = signed.signature.hex()
    
    response = client.post("/logs/ingest", json={
        "logs": [{
            "device_id": "ESP32-TEST001",
            "timestamp": timestamp,
            "level": "INFO",
            "category": "motion",
            "message": message,
            "signature": signature_hex
        }]
    })
    
    assert response.status_code == 200
    data = response.json()
    assert data["status"] == "ok"
    assert data["ingested"] == 1
    assert data["errors"] == 0


def test_ingest_invalid_signature(client, test_device):
    """Test that logs with invalid signatures are rejected."""
    timestamp = 1704268800
    message = "Motion detected"
    
    response = client.post("/logs/ingest", json={
        "logs": [{
            "device_id": "ESP32-TEST001",
            "timestamp": timestamp,
            "level": "INFO",
            "category": "motion",
            "message": message,
            "signature": "0000000000000000000000000000000000000000000000000000000000000000"
                         "0000000000000000000000000000000000000000000000000000000000000000"
        }]
    })
    
    assert response.status_code == 200
    data = response.json()
    assert data["ingested"] == 0
    assert data["errors"] == 1


def test_ingest_unknown_device(client):
    """Test that logs from unknown devices are rejected."""
    timestamp = 1704268800
    message = "Motion detected"
    
    response = client.post("/logs/ingest", json={
        "logs": [{
            "device_id": "ESP32-UNKNOWN",
            "timestamp": timestamp,
            "level": "INFO",
            "category": "motion",
            "message": message,
            "signature": "abc123def456"
        }]
    })
    
    assert response.status_code == 200
    data = response.json()
    assert data["ingested"] == 0
    assert data["errors"] == 1


def test_ingest_batch(client, test_device, test_keypair):
    """Test ingesting multiple logs in a batch."""
    logs_batch = []
    
    for i in range(3):
        timestamp = 1704268800 + i
        message = f"Motion event {i}"
        
        message_to_sign = f"{int(timestamp)}:{message}".encode('utf-8')
        signed = test_keypair["signing_key"].sign(message_to_sign)
        signature_hex = signed.signature.hex()
        
        logs_batch.append({
            "device_id": "ESP32-TEST001",
            "timestamp": timestamp,
            "level": "INFO",
            "category": "motion",
            "message": message,
            "signature": signature_hex
        })
    
    response = client.post("/logs/ingest", json={"logs": logs_batch})
    
    assert response.status_code == 200
    data = response.json()
    assert data["status"] == "ok"
    assert data["ingested"] == 3
    assert data["errors"] == 0


def test_get_logs(client, test_device, test_keypair):
    """Test retrieving logs for a device."""
    # First ingest a log
    timestamp = 1704268800
    message = "Test log entry"
    
    message_to_sign = f"{int(timestamp)}:{message}".encode('utf-8')
    signed = test_keypair["signing_key"].sign(message_to_sign)
    signature_hex = signed.signature.hex()
    
    client.post("/logs/ingest", json={
        "logs": [{
            "device_id": "ESP32-TEST001",
            "timestamp": timestamp,
            "level": "INFO",
            "category": "motion",
            "message": message,
            "signature": signature_hex
        }]
    })
    
    # Now retrieve logs
    response = client.get("/logs?device_id=ESP32-TEST001&limit=10")
    
    assert response.status_code == 200
    logs = response.json()
    assert len(logs) > 0
    assert logs[0]["device_id"] == "ESP32-TEST001"
    assert logs[0]["message"] == message


def test_get_logs_limit(client, test_device, test_keypair):
    """Test log retrieval respects limit parameter."""
    # Ingest 5 logs
    for i in range(5):
        timestamp = 1704268800 + i
        message = f"Log {i}"
        
        message_to_sign = f"{int(timestamp)}:{message}".encode('utf-8')
        signed = test_keypair["signing_key"].sign(message_to_sign)
        signature_hex = signed.signature.hex()
        
        client.post("/logs/ingest", json={
            "logs": [{
                "device_id": "ESP32-TEST001",
                "timestamp": timestamp,
                "level": "INFO",
                "category": "motion",
                "message": message,
                "signature": signature_hex
            }]
        })
    
    # Retrieve with limit=2
    response = client.get("/logs?device_id=ESP32-TEST001&limit=2")
    
    assert response.status_code == 200
    logs = response.json()
    assert len(logs) == 2
