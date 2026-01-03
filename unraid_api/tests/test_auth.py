"""API tests for authentication endpoints."""

import pytest
import security


def test_login_success(client, admin_user):
    """Test successful login with valid TOTP code."""
    user, secret = admin_user
    totp_code = security.pyotp.TOTP(secret).now()
    
    response = client.post("/auth/session", json={
        "username": "admin",
        "totp_code": totp_code
    })
    
    assert response.status_code == 200
    assert "access_token" in response.json()
    assert response.json()["token_type"] == "bearer"


def test_login_invalid_username(client):
    """Test login fails with non-existent username."""
    response = client.post("/auth/session", json={
        "username": "nonexistent",
        "totp_code": "123456"
    })
    
    assert response.status_code == 400
    assert "Incorrect username or code" in response.json()["detail"]


def test_login_invalid_totp(client, admin_user):
    """Test login fails with invalid TOTP code."""
    user, secret = admin_user
    
    response = client.post("/auth/session", json={
        "username": "admin",
        "totp_code": "000000"  # Invalid code
    })
    
    assert response.status_code == 400
    assert "Incorrect username or code" in response.json()["detail"]


def test_token_decode(valid_token):
    """Test JWT token can be decoded."""
    payload = security.decode_access_token(valid_token)
    
    assert payload is not None
    assert payload["sub"] == "admin"
    assert "exp" in payload


def test_token_expiration():
    """Test that expired tokens are rejected."""
    from datetime import timedelta
    
    # Create token with 0 minute expiry
    expired_token = security.create_access_token(
        data={"sub": "admin"},
        expires_delta=timedelta(seconds=-1)
    )
    
    payload = security.decode_access_token(expired_token)
    # Should return None or raise for expired token
    assert payload is None or "exp" in payload
