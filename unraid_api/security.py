import nacl.signing
import nacl.encoding
import nacl.exceptions
import pyotp
from datetime import datetime, timedelta
from jose import jwt, JWTError
from typing import Optional
import os

# Configuration
# Load secret key from environment variable, default to a fallback for development (warn user)
SECRET_KEY = os.getenv("UNRAID_API_SECRET_KEY", "CHANGE_THIS_TO_A_SECURE_RANDOM_STRING_IF_NOT_DEV")
ALGORITHM = "HS256"
ACCESS_TOKEN_EXPIRE_MINUTES = 15

def verify_signature(public_key_hex: str, message: bytes, signature: bytes) -> bool:
    """Verifies an Ed25519 signature."""
    try:
        verify_key = nacl.signing.VerifyKey(public_key_hex, encoder=nacl.encoding.HexEncoder)
        verify_key.verify(message, signature)
        return True
    except nacl.exceptions.BadSignatureError:
        return False
    except Exception as e:
        print(f"Verification error: {e}")
        return False

def generate_totp_secret() -> str:
    return pyotp.random_base32()

def verify_totp(secret: str, code: str) -> bool:
    totp = pyotp.TOTP(secret)
    return totp.verify(code)

def create_access_token(data: dict, expires_delta: Optional[timedelta] = None):
    to_encode = data.copy()
    if expires_delta:
        expire = datetime.utcnow() + expires_delta
    else:
        expire = datetime.utcnow() + timedelta(minutes=15)
    to_encode.update({"exp": expire})
    encoded_jwt = jwt.encode(to_encode, SECRET_KEY, algorithm=ALGORITHM)
    return encoded_jwt

def decode_access_token(token: str):
    try:
        payload = jwt.decode(token, SECRET_KEY, algorithms=[ALGORITHM])
        return payload
    except JWTError:
        return None

def validate_command_nonce(db, network_id: int, nonce: str, timestamp: datetime) -> tuple[bool, Optional[str]]:
    """
    Validates command nonce to prevent replay attacks.

    Args:
        db: Database session
        network_id: Network ID for the command
        nonce: Unique nonce value
        timestamp: Command timestamp

    Returns:
        (is_valid, error_message) tuple
        - (True, None) if nonce is valid
        - (False, error_message) if nonce is invalid/reused or timestamp is stale
    """
    from models import CommandNonce

    # Check timestamp is within 5-minute window
    now = datetime.utcnow()
    time_diff = abs((now - timestamp).total_seconds())

    if time_diff > 300:  # 5 minutes
        return False, f"Command timestamp is stale (>5 minutes old)"

    # Check if nonce already exists (replay attack)
    existing_nonce = db.query(CommandNonce).filter(CommandNonce.nonce == nonce).first()
    if existing_nonce:
        return False, "Nonce has already been used (replay attack detected)"

    # Store nonce in database
    try:
        new_nonce = CommandNonce(
            nonce=nonce,
            timestamp=timestamp,
            network_id=network_id
        )
        db.add(new_nonce)
        db.commit()
        return True, None
    except Exception as e:
        db.rollback()
        return False, f"Failed to store nonce: {str(e)}"

def cleanup_old_nonces(db, max_age_minutes: int = 10):
    """
    Cleanup old nonces to prevent database bloat.
    Should be called periodically by background task.

    Args:
        db: Database session
        max_age_minutes: Delete nonces older than this many minutes
    """
    from models import CommandNonce

    cutoff_time = datetime.utcnow() - timedelta(minutes=max_age_minutes)
    db.query(CommandNonce).filter(CommandNonce.timestamp < cutoff_time).delete()
    db.commit()
