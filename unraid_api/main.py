from fastapi import FastAPI, Depends, HTTPException, status, WebSocket
from sqlalchemy.orm import Session
from typing import List, Optional
from pydantic import BaseModel
from datetime import datetime
import models
import security
from models import SessionLocal, init_db
import os
import json
import nacl.encoding
import nacl.signing

# --- Pydantic Models ---
class NetworkCreate(BaseModel):
    name: str

class DeviceCreate(BaseModel):
    device_id: str
    network_id: int
    type: str
    public_key: str

class LogIngestItem(BaseModel):
    device_id: str
    timestamp: float
    level: str
    category: str
    message: str
    signature: str # Hex signature of the log payload

class LogIngestRequest(BaseModel):
    logs: List[LogIngestItem]

class CommandRequest(BaseModel):
    command: str
    payload: dict

class Token(BaseModel):
    access_token: str
    token_type: str

class LoginRequest(BaseModel):
    username: str
    totp_code: str

# --- Dependency ---
def get_db():
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()

# --- App Init ---
os.makedirs("./data", exist_ok=True)
init_db()
app = FastAPI(title="Unraid Central API")

# --- Routes ---

@app.post("/auth/session", response_model=Token)
def login(request: LoginRequest, db: Session = Depends(get_db)):
    user = db.query(models.User).filter(models.User.username == request.username).first()
    if not user:
        raise HTTPException(status_code=400, detail="Incorrect username or code")

    if not security.verify_totp(user.totp_secret, request.totp_code):
         raise HTTPException(status_code=400, detail="Incorrect username or code")

    access_token = security.create_access_token(data={"sub": user.username})
    return {"access_token": access_token, "token_type": "bearer"}

@app.post("/networks")
def create_network(network: NetworkCreate, db: Session = Depends(get_db)):
    # In a real app, require auth here
    db_network = models.Network(name=network.name)
    # Generate keys for the network
    signing_key = security.nacl.signing.SigningKey.generate()
    db_network.public_key = signing_key.verify_key.encode(encoder=security.nacl.encoding.HexEncoder).decode()
    db_network.private_key_enc = signing_key.encode(encoder=security.nacl.encoding.HexEncoder).decode() # Store insecurely for demo

    db.add(db_network)
    db.commit()
    db.refresh(db_network)
    return db_network

@app.get("/networks")
def list_networks(db: Session = Depends(get_db)):
    return db.query(models.Network).all()

@app.post("/networks/{network_id}/devices")
def register_device(network_id: int, device: DeviceCreate, db: Session = Depends(get_db)):
    db_device = models.Device(
        device_id=device.device_id,
        network_id=network_id,
        type=device.type,
        public_key=device.public_key
    )
    db.add(db_device)
    db.commit()
    return {"status": "registered"}

@app.get("/networks/{network_id}/devices")
def list_devices(network_id: int, db: Session = Depends(get_db)):
    return db.query(models.Device).filter(models.Device.network_id == network_id).all()

@app.post("/logs/ingest")
def ingest_logs(batch: LogIngestRequest, db: Session = Depends(get_db)):
    count = 0
    errors = 0
    for log_item in batch.logs:
        # 1. Fetch device public key
        device = db.query(models.Device).filter(models.Device.device_id == log_item.device_id).first()
        if not device:
            print(f"Device {log_item.device_id} not found")
            errors += 1
            continue

        # 2. Reconstruct payload string to verify signature
        # This format MUST match exactly what the firmware signs (usually JSON string or concatenated fields)
        # For this implementation, we assume the firmware signs the 'message' content + timestamp
        # Message to sign: "{timestamp}:{message}"
        # Cast timestamp to int to match C firmware (uint32_t)
        message_to_verify = f"{int(log_item.timestamp)}:{log_item.message}".encode('utf-8')

        # 3. Verify signature
        if not security.verify_signature(device.public_key, message_to_verify, bytes.fromhex(log_item.signature)):
            print(f"Invalid signature for log from {log_item.device_id}")
            errors += 1
            continue

        # Storing log
        log_entry = models.DeviceLog(
            device_id=log_item.device_id,
            timestamp=datetime.fromtimestamp(log_item.timestamp),
            level=log_item.level,
            category=log_item.category,
            message=log_item.message
        )
        db.add(log_entry)
        count += 1

    db.commit()
    return {"status": "ok", "ingested": count, "errors": errors}

@app.get("/logs")
def get_logs(device_id: Optional[str] = None, limit: int = 100, db: Session = Depends(get_db)):
    query = db.query(models.DeviceLog)
    if device_id:
        query = query.filter(models.DeviceLog.device_id == device_id)
    return query.order_by(models.DeviceLog.timestamp.desc()).limit(limit).all()

@app.post("/networks/{network_id}/command")
def send_command(network_id: int, cmd: CommandRequest, db: Session = Depends(get_db)):
    network = db.query(models.Network).filter(models.Network.id == network_id).first()
    if not network:
        raise HTTPException(status_code=404, detail="Network not found")

    # 1. Fetch network private key
    try:
        signing_key = nacl.signing.SigningKey(network.private_key_enc, encoder=nacl.encoding.HexEncoder)
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Key error: {e}")

    # 2. Sign command payload
    # Format: "{timestamp}:{command}:{json_payload}"
    timestamp = int(datetime.utcnow().timestamp())
    payload_str = json.dumps(cmd.payload)
    message_to_sign = f"{timestamp}:{cmd.command}:{payload_str}".encode('utf-8')

    signed = signing_key.sign(message_to_sign)
    signature_hex = signed.signature.hex()

    # 3. Return the signed command bundle (in real system, push to WS)
    return {
        "status": "command_queued",
        "command_bundle": {
            "network_id": network_id,
            "timestamp": timestamp,
            "command": cmd.command,
            "payload": cmd.payload,
            "signature": signature_hex
        }
    }

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    await websocket.accept()
    while True:
        data = await websocket.receive_text()
        await websocket.send_text(f"Message text was: {data}")
