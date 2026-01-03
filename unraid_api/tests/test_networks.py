"""API tests for network management endpoints."""

import pytest


def test_create_network(client, valid_token):
    """Test network creation with valid session token."""
    response = client.post(
        "/networks",
        json={"name": "new-network"},
        headers={"Authorization": f"Bearer {valid_token}"}
    )
    
    assert response.status_code == 200
    data = response.json()
    assert data["name"] == "new-network"
    assert "public_key" in data
    assert "id" in data


def test_list_networks(client):
    """Test listing all networks."""
    response = client.get("/networks")
    
    assert response.status_code == 200
    assert isinstance(response.json(), list)


def test_list_networks_includes_test_network(client, test_network):
    """Test that created networks appear in list."""
    response = client.get("/networks")
    
    assert response.status_code == 200
    networks = response.json()
    assert len(networks) > 0
    
    network_names = [n["name"] for n in networks]
    assert "test-network-1" in network_names


def test_register_device(client, test_network, test_keypair, valid_token):
    """Test device registration in a network."""
    response = client.post(
        f"/networks/{test_network.id}/devices",
        json={
            "device_id": "ESP32-NEW001",
            "network_id": test_network.id,
            "type": "motion",
            "public_key": test_keypair["public_key"]
        },
        headers={"Authorization": f"Bearer {valid_token}"}
    )
    
    assert response.status_code == 200
    assert response.json()["status"] == "registered"


def test_list_devices(client, test_network, test_device):
    """Test listing devices in a network."""
    response = client.get(f"/networks/{test_network.id}/devices")
    
    assert response.status_code == 200
    devices = response.json()
    assert len(devices) > 0
    
    device_ids = [d["device_id"] for d in devices]
    assert "ESP32-TEST001" in device_ids


def test_list_devices_by_type(client, test_network, test_device):
    """Test that device type is correctly returned."""
    response = client.get(f"/networks/{test_network.id}/devices")
    
    assert response.status_code == 200
    devices = response.json()
    
    test_dev = next(d for d in devices if d["device_id"] == "ESP32-TEST001")
    assert test_dev["type"] == "motion"
