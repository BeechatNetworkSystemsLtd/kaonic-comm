from contextlib import asynccontextmanager
from cryptography import x509
from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import rsa, ed25519, padding
from fastapi import FastAPI, UploadFile, File, HTTPException
from fastapi.responses import JSONResponse
from pathlib import Path

import hashlib
import logging
import os
import shutil
import stat
import subprocess
import tempfile
import time
import uvicorn
import zipfile

BINARY_PATH = Path("/usr/bin")
APP_PATH = BINARY_PATH / "kaonic-commd"

METADATA_PATH = Path("/etc/kaonic")
VERSION_PATH = METADATA_PATH / "kaonic-commd.version"
HASH_PATH = METADATA_PATH / "kaonic-commd.sha256"
BACKUP_PATH = METADATA_PATH / "kaonic-commd.bak"
CERT_PATH = METADATA_PATH / "kaonic-commd.pem"

logger = logging.getLogger("uvicorn.error")

@asynccontextmanager
async def lifespan(app: FastAPI):
    validate_app_file()
    yield

app = FastAPI(lifespan=lifespan)

def validate_app_file():
    if not METADATA_PATH.exists():
        logger.info("Metadata path is not existing. Creating empty one")
        METADATA_PATH.mkdir(parents=True)
        return

    if not APP_PATH.exists() and not BACKUP_PATH.exists():
        logger.info("No kaonic_commd and kaonic_commd.bak")
        return

    expected_hash = HASH_PATH.read_text().strip() if HASH_PATH.exists() else ""
    actual_hash = sha256sum(APP_PATH) if APP_PATH.exists() else ""

    if (not APP_PATH.exists() or not HASH_PATH.exists()) or (expected_hash != actual_hash):
        logger.info("Restoring app from the backup")
        stop_kaonic_commd()
        restore_backup()
        launch_kaonic_commd()

def validate_signature(binary_path, signature_path, certificate_path):
    try:
        cert = x509.load_pem_x509_certificate(certificate_path.read_bytes())
        public_key = cert.public_key()

        message = binary_path.read_bytes()
        signature = signature_path.read_bytes()

        if isinstance(public_key, rsa.RSAPublicKey):
            public_key.verify(
                signature,
                message,
                padding.PKCS1v15(),
                hashes.SHA256()
            )
        elif isinstance(public_key, ed25519.Ed25519PublicKey):
            public_key.verify(signature, message)
        else:
            logger.error(f"Unsupported key type in X.509: {type(public_key)}")
            return False

        return True

    except InvalidSignature:
        logger.error("Signature verification failed")
        return False
    except Exception as e:
        logger.error(f"Error during signature verification: {e}")
        return False

@app.post("/api/ota/commd/upload")
async def upload_ota(file: UploadFile = File(...)):
    if file.content_type != "application/x-zip-compressed":
        raise HTTPException(status_code=400, detail="Only ZIP files accepted")
    
    if not CERT_PATH.exists():
        raise HTTPException(status_code=500, detail="OTA certificate is not present")

    with tempfile.TemporaryDirectory() as tmp_dir_str:
        temp_dir = Path(tmp_dir_str)
        zip_path = temp_dir / "upload.zip"
        app_path = temp_dir / "kaonic-commd"
        hash_path = temp_dir / "kaonic-commd.sha256"
        sign_path = temp_dir / "kaonic-commd.sig"
        version_path = temp_dir / "kaonic-commd.version"

        with zip_path.open("wb") as buffer:
            shutil.copyfileobj(file.file, buffer)

        try:
            logger.info("Uploaded zip extraction")
            with zipfile.ZipFile(zip_path, "r") as zip_ref:
                zip_ref.extractall(temp_dir)
        except zipfile.BadZipFile:
            logger.error("Zip wasn't extracted")
            raise HTTPException(status_code=400, detail="Invalid ZIP file")

        logger.info("Zip content validation")
        required_files = ["kaonic-commd", "kaonic-commd.sha256", "kaonic-commd.version"]
        for req_file in required_files:
            if not (temp_dir / req_file).exists():
                logger.error(f"Missing {req_file} in OTA package")
                raise HTTPException(status_code=400, detail=f"Missing {req_file} in OTA package")

        expected_hash = (hash_path).read_text().strip()
        actual_hash = sha256sum(app_path)

        logger.info(f"Expected hash: {expected_hash}")
        logger.info(f"Actual hash: {actual_hash}")

        if expected_hash != actual_hash:
            logger.error("SHA256 hash mismatch")
            raise HTTPException(status_code=400, detail="SHA256 hash mismatch")
        
        if not validate_signature(app_path, sign_path, CERT_PATH):
            logger.error("Signature wasn't validated")
            raise HTTPException(status_code=400, detail="SHA256 hash mismatch")

        stop_kaonic_commd()
        backup_current()

        shutil.copy2(app_path, APP_PATH)

        logger.info("Changing app file permissions")
        current_permissions = APP_PATH.stat().st_mode
        os.chmod(APP_PATH, current_permissions | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)

        launch_kaonic_commd()

        logger.info("Checking kaonic-commd service status via systemctl")
        for _ in range(10):
            if is_kaonic_running():
                time.sleep(1)
            else:
                logger.error("Application validation failed")
                break

        if is_kaonic_running():
            shutil.copy2(hash_path, HASH_PATH)
            shutil.copy2(version_path, VERSION_PATH)
            logger.info("Updated successfully")
            return {"detail": "Update successful"}
        else:
            restore_backup()
            launch_kaonic_commd()
            logger.error("Failed to start new app, rollback done")
            raise HTTPException(status_code=500, detail="Failed to start new app, rollback done")

@app.get("/api/ota/commd/version")
def get_version():
    if not VERSION_PATH.exists() or not HASH_PATH.exists():
        return JSONResponse(content={"version": None, "hash": None})

    version = VERSION_PATH.read_text().strip()
    hash_val = HASH_PATH.read_text().strip()
    return {"version": version, "hash": hash_val}

def sha256sum(file_path: Path) -> str:
    h = hashlib.sha256()
    with file_path.open("rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            h.update(chunk)
    return h.hexdigest()

def stop_kaonic_commd():
    logger.info("Stopping kaonic-commd service via systemctl")
    subprocess.run(
        ["systemctl", "stop", "kaonic-commd.service"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False
    )

def is_kaonic_running() -> bool:
    result = subprocess.run(
        ["systemctl", "is-active", "--quiet", "kaonic-commd.service"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL
    )
    return result.returncode == 0

def launch_kaonic_commd():
    logger.info("Starting kaonic-commd service via systemctl")
    subprocess.run(
        ["systemctl", "start", "kaonic-commd.service"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False
    )

def backup_current():
    logger.info("Backup current executable")

    if BACKUP_PATH.exists():
        BACKUP_PATH.unlink()

    if APP_PATH.exists():
        shutil.copy2(APP_PATH, BACKUP_PATH)


def restore_backup():
    logger.info("Restoring backup")
    if BACKUP_PATH.exists():
        shutil.copy2(BACKUP_PATH, APP_PATH)
        logger.info("Backup restored")
    else:
        logger.info("Backup file is absent")

if __name__ == "__main__":
    uvicorn.run(
        "kaonic-ota:app",
        host="0.0.0.0",
        port=8682,
        reload=True
    )
