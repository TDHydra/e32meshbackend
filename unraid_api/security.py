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
