from sqlalchemy import create_engine, Column, Integer, String, Boolean, ForeignKey, DateTime, Text
from sqlalchemy.orm import declarative_base, relationship, sessionmaker
from datetime import datetime

DATABASE_URL = "sqlite:///./data/unraid_central.db"

engine = create_engine(DATABASE_URL, connect_args={"check_same_thread": False})
SessionLocal = sessionmaker(autocommit=False, autoflush=False, bind=engine)

Base = declarative_base()

class User(Base):
    __tablename__ = "users"
    id = Column(Integer, primary_key=True, index=True)
    username = Column(String, unique=True, index=True)
    hashed_password = Column(String) # For basic auth if needed, primarily relying on TOTP for session
    totp_secret = Column(String)
    verified = Column(Boolean, default=False)

class Network(Base):
    __tablename__ = "networks"
    id = Column(Integer, primary_key=True, index=True)
    name = Column(String, unique=True, index=True)
    home_base_ip = Column(String, nullable=True)
    public_key = Column(String) # Hex encoded Ed25519 public key
    private_key_enc = Column(String) # Encrypted private key (optional, for management)

    devices = relationship("Device", back_populates="network")

class Device(Base):
    __tablename__ = "devices"
    id = Column(Integer, primary_key=True, index=True) # Internal DB ID
    device_id = Column(String, unique=True, index=True) # Hardware ID / MAC
    network_id = Column(Integer, ForeignKey("networks.id"))
    type = Column(String)
    public_key = Column(String)
    config = Column(Text, default="{}")
    online = Column(Boolean, default=False)
    last_seen = Column(DateTime, default=datetime.utcnow)

    network = relationship("Network", back_populates="devices")
    logs = relationship("DeviceLog", back_populates="device")
    motion_events = relationship("MotionEvent", back_populates="device")

class DeviceLog(Base):
    __tablename__ = "device_logs"
    id = Column(Integer, primary_key=True, index=True)
    device_id = Column(String, ForeignKey("devices.device_id"))
    timestamp = Column(DateTime, default=datetime.utcnow)
    level = Column(String)
    category = Column(String)
    message = Column(Text)

    device = relationship("Device", back_populates="logs")

class MotionEvent(Base):
    __tablename__ = "motion_events"
    id = Column(Integer, primary_key=True, index=True)
    device_id = Column(String, ForeignKey("devices.device_id"))
    timestamp = Column(DateTime, default=datetime.utcnow)
    media_path = Column(String)

    device = relationship("Device", back_populates="motion_events")

class Firmware(Base):
    __tablename__ = "firmware"
    id = Column(Integer, primary_key=True, index=True)
    version = Column(String, unique=True, index=True)
    chip = Column(String)
    type = Column(String)
    file_hash = Column(String)
    signature = Column(String)
    path = Column(String)
    created_at = Column(DateTime, default=datetime.utcnow)

class CommandNonce(Base):
    __tablename__ = "command_nonces"
    id = Column(Integer, primary_key=True, index=True)
    nonce = Column(String, unique=True, index=True)
    timestamp = Column(DateTime, index=True)
    network_id = Column(Integer, ForeignKey("networks.id"))

def init_db():
    Base.metadata.create_all(bind=engine)
