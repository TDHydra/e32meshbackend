# Running API Tests

## Setup

1. Install test dependencies:
```bash
pip install pytest pytest-cov
```

2. Run all tests:
```bash
pytest
```

3. Run with coverage:
```bash
pytest --cov=./ --cov-report=html
```

4. Run specific test file:
```bash
pytest tests/test_auth.py -v
```

5. Run specific test:
```bash
pytest tests/test_auth.py::test_login_success -v
```

## Test Structure

- `conftest.py` - Pytest fixtures for database, client, and test data
- `test_auth.py` - Authentication and token tests
- `test_networks.py` - Network creation and device registration tests
- `test_logs.py` - Log ingestion and signature verification tests
- `test_commands.py` - Command delivery and signing tests

## Test Fixtures

All tests use the following fixtures from `conftest.py`:

- `db_session` - In-memory SQLite database
- `client` - FastAPI TestClient
- `admin_user` - Pre-created admin user with TOTP secret
- `valid_token` - Valid JWT session token
- `test_keypair` - Generated Ed25519 keypair for devices
- `test_network` - Pre-created network in database
- `test_device` - Pre-created device in test network

## Key Test Data

**Admin User**: 
- username: `admin`
- TOTP secret: Generated per test

**Test Network**:
- id: Auto-assigned
- name: `test-network-1`
- public_key: `abc123def456`

**Test Device**:
- device_id: `ESP32-TEST001`
- type: `motion`
- public_key: Generated from test keypair

## Critical Test Patterns

### Signature Verification
All log ingestion tests verify the critical `"{timestamp}:{message}"` signing format:

```python
message_to_sign = f"{int(timestamp)}:{message}".encode('utf-8')
signed = keypair["signing_key"].sign(message_to_sign)
signature_hex = signed.signature.hex()
```

This MUST match the firmware implementation exactly.

### Batch Operations
Log ingestion tests include batch scenarios (multiple logs in single request) and single log scenarios.

### Error Cases
Each endpoint is tested with:
- Valid input (success case)
- Invalid signatures
- Missing resources
- Authentication failures (when applicable)

## CI/CD Integration

Add to your CI/CD pipeline:
```bash
pytest --cov --cov-report=xml
```

This generates coverage report for code quality metrics.
