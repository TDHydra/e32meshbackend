# Authentication Middleware TODO

Reference for implementing HIGH PRIORITY authentication completion item.

## Current State

### ✅ Already Requires Auth
- `POST /networks/{id}/command` - Has `current_user: str = Depends(middleware.get_current_user)`

### ❌ Needs Auth (Missing)
```python
# In main.py, add to these endpoints:

@app.post("/networks")
def create_network(
    network: NetworkCreate, 
    db: Session = Depends(get_db),
    current_user: str = Depends(middleware.get_current_user)  # ADD THIS
):
    # Enforce auth before creating network
    ...

@app.post("/networks/{network_id}/devices")
def register_device(
    network_id: int, 
    device: DeviceCreate, 
    db: Session = Depends(get_db),
    current_user: str = Depends(middleware.get_current_user)  # ADD THIS
):
    # Only authenticated users can register devices
    ...

@app.post("/logs/ingest")
def ingest_logs(
    batch: LogIngestRequest, 
    db: Session = Depends(get_db),
    # NOTE: Home base firmware will send logs
    # May need different auth (API key?) than web UI endpoints
):
    # TODO: Decide on auth strategy for home base
    # Option 1: X-API-Key header from home base
    # Option 2: Separate endpoint /logs/ingest (firmware) vs /logs (UI)
    ...
```

## Implementation Steps

### 1. Create middleware.py (if not exists)
```python
from fastapi import Depends, HTTPException, status
from jose import JWTError
import security

async def get_current_user(token: str = Header(...)) -> str:
    """Extract username from valid JWT token in Authorization header."""
    # Expect: Authorization: Bearer <token>
    if not token.startswith("Bearer "):
        raise HTTPException(status_code=401, detail="Invalid token format")
    
    jwt_token = token[7:]  # Remove "Bearer " prefix
    payload = security.decode_access_token(jwt_token)
    
    if payload is None:
        raise HTTPException(status_code=401, detail="Invalid or expired token")
    
    return payload.get("sub")
```

### 2. Add Import to main.py
```python
import middleware  # Add to imports
```

### 3. Update Endpoints (See Above)
Add `current_user: str = Depends(middleware.get_current_user)` to:
- POST /networks
- POST /networks/{id}/devices
- Decide on /logs/ingest auth strategy

### 4. Validate with Tests
```bash
cd unraid_api
pip install pytest
pytest tests/test_networks.py::test_create_network -v
# Should show 403 Unauthorized without token
# Should show 200 with valid token
```

## Test Validation

The test suite already has `valid_token` fixture that tests can use:

```python
def test_create_network_requires_auth(client):
    """Network creation must require valid token."""
    response = client.post("/networks", json={"name": "test"})
    assert response.status_code == 403  # Forbidden without token

def test_create_network_with_auth(client, valid_token):
    """Network creation succeeds with valid token."""
    response = client.post(
        "/networks",
        json={"name": "test"},
        headers={"Authorization": f"Bearer {valid_token}"}
    )
    assert response.status_code == 200
```

## Header Format

All authenticated endpoints expect:
```
Authorization: Bearer <jwt_token>
```

Example (from test):
```python
client.post(
    "/networks",
    headers={"Authorization": f"Bearer {valid_token}"}
)
```

## Firmware Authentication (Special Case)

Home base firmware sending logs to `/logs/ingest` may need different auth:

**Option A: API Key**
```python
@app.post("/logs/ingest")
def ingest_logs(
    batch: LogIngestRequest,
    db: Session = Depends(get_db),
    api_key: str = Header(...)  # X-API-Key
):
    if api_key != FIRMWARE_API_KEY:
        raise HTTPException(status_code=401)
```

**Option B: Separate Endpoint**
```python
@app.post("/api/logs")  # Internal firmware endpoint
def ingest_logs_firmware(batch: LogIngestRequest):
    # No auth required (local network only)
    ...

@app.get("/logs")  # Public UI endpoint
def get_logs_ui(
    current_user: str = Depends(middleware.get_current_user)
):
    # Requires auth
    ...
```

**Recommendation**: Option A with `X-API-Key` header from home base config.

## Files to Modify

1. `unraid_api/main.py` - Add middleware dependency to 3 endpoints
2. `unraid_api/middleware.py` - Create (if missing) or update
3. `unraid_api/tests/test_*.py` - Already has test cases

## Progress Tracking

- [ ] Create/update middleware.py
- [ ] Add dependency to POST /networks
- [ ] Add dependency to POST /networks/{id}/devices  
- [ ] Decide on /logs/ingest auth strategy
- [ ] Add dependency to POST /logs/ingest (or create separate endpoint)
- [ ] Run: `pytest tests/ -v` (all 24 tests should pass)
- [ ] Test manually with curl: `curl -H "Authorization: Bearer $(generate_token)" http://localhost:8000/networks`

## Reference

- JWT Token Format: See `security.py::create_access_token()`
- Test Fixtures: See `unraid_api/tests/conftest.py::valid_token`
- Current Implementation: See `main.py::send_command()` for reference
