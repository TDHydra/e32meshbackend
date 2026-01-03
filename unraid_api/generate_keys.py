import pyotp
import qrcode
import os
from models import SessionLocal, User, init_db

def setup_admin():
    db = SessionLocal()
    init_db()

    username = input("Enter admin username: ")
    existing = db.query(User).filter(User.username == username).first()
    if existing:
        print("User already exists.")
        return

    secret = pyotp.random_base32()
    user = User(username=username, totp_secret=secret, verified=True)
    db.add(user)
    db.commit()

    print(f"\nUser '{username}' created.")
    print(f"TOTP Secret: {secret}")

    # Generate QR code URL
    uri = pyotp.totp.TOTP(secret).provisioning_uri(name=username, issuer_name="UnraidCentral")
    print(f"TOTP QR Code URL (paste in browser to view): https://api.qrserver.com/v1/create-qr-code/?data={uri}&size=200x200")

    print("\nScan this into Google Authenticator.")

if __name__ == "__main__":
    setup_admin()
