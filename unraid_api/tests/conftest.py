"""Pytest configuration and fixtures for API tests."""

import os
import tempfile
import pytest
from fastapi.testclient import TestClient
from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker
from sqlalchemy.pool import StaticPool

# Add parent directory to path for imports
import sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import models
from main import app, get_db
import security
import nacl.signing
import nacl.encoding


@pytest.fixture(scope="function")
def db_engine():
    """Create a test database engine with StaticPool for thread safety."""
    engine = create_engine(
        "sqlite:///:memory:",
        connect_args={"check_same_thread": False},
        poolclass=StaticPool,
    )
    models.Base.metadata.create_all(bind=engine)
    yield engine
    engine.dispose()


@pytest.fixture(scope="function")
def db_session(db_engine):
    """Create a database session for each test."""
    SessionLocal = sessionmaker(autocommit=False, autoflush=False, bind=db_engine)
    session = SessionLocal()
    yield session
    session.close()


@pytest.fixture(scope="function")
def client(db_engine):
    """Create FastAPI test client with test database."""
    SessionLocal = sessionmaker(autocommit=False, autoflush=False, bind=db_engine)
    
    def override_get_db():
        db = SessionLocal()
        try:
            yield db
        finally:
            db.close()
    
    app.dependency_overrides[get_db] = override_get_db
    with TestClient(app) as test_client:
        yield test_client
    app.dependency_overrides.clear()


@pytest.fixture
def admin_user(db_session):
    """Create admin user with TOTP secret."""
    secret = security.generate_totp_secret()
    user = models.User(
        username="admin",
        totp_secret=secret,
        verified=True
    )
    db_session.add(user)
    db_session.commit()
    return user, secret


@pytest.fixture
def valid_token(client, admin_user):
    """Get valid session token for admin user."""
    user, secret = admin_user
    totp = security.verify_totp(secret, security.pyotp.TOTP(secret).now())
    
    # Note: In real tests, generate TOTP code properly
    # For now, we'll manually create token
    token = security.create_access_token(data={"sub": "admin"})
    return token


@pytest.fixture
def test_keypair():
    """Generate Ed25519 keypair for test device."""
    sk = nacl.signing.SigningKey.generate()
    public_key = sk.verify_key.encode(encoder=nacl.encoding.HexEncoder).decode()
    private_key = sk.encode(encoder=nacl.encoding.HexEncoder).decode()
    return {
        "public_key": public_key,
        "private_key": private_key,
        "signing_key": sk
    }


@pytest.fixture
def test_network(db_session):
    """Create a test network with valid Ed25519 keypair."""
    # Generate a real Ed25519 keypair for the network
    signing_key = nacl.signing.SigningKey.generate()
    public_key = signing_key.verify_key.encode(encoder=nacl.encoding.HexEncoder).decode()
    private_key = signing_key.encode(encoder=nacl.encoding.HexEncoder).decode()
    
    network = models.Network(
        name="test-network-1",
        public_key=public_key,
        private_key_enc=private_key
    )
    db_session.add(network)
    db_session.commit()
    return network


@pytest.fixture
def test_device(db_session, test_network, test_keypair):
    """Create a test device."""
    device = models.Device(
        device_id="ESP32-TEST001",
        network_id=test_network.id,
        type="motion",
        public_key=test_keypair["public_key"]
    )
    db_session.add(device)
    db_session.commit()
    return device
