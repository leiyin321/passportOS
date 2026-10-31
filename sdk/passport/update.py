"""Signed Passport OS firmware update manifest tooling."""
import hashlib
import json
import re
import subprocess
import tempfile
from pathlib import Path
from .files import atomic_write

MAX_APP_IMAGE = 2_621_440
MAX_BUILD = 2_147_483_647


def canonical_manifest(version, build, size, sha256, image):
    if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", version):
        raise ValueError("version must be numeric semver")
    if type(build) is not int or not 1 <= build <= MAX_BUILD:
        raise ValueError("build must be a positive monotonic integer <= 2^31-1")
    if type(size) is not int or not 4096 <= size <= MAX_APP_IMAGE:
        raise ValueError("application image size is outside the OTA slot")
    if not re.fullmatch(r"[A-Za-z0-9._-]{1,96}", image) or image in (".", ".."):
        raise ValueError("image must be a safe filename")
    if not re.fullmatch(r"[0-9a-f]{64}", sha256):
        raise ValueError("invalid SHA-256")
    return (f"passport-firmware/1\nchip=esp32c3\nversion={version}\n"
            f"build={build}\nsize={size}\nsha256={sha256}\nimage={image}\n").encode()


def _openssl(args, data=None):
    result = subprocess.run(["openssl", *map(str, args)], input=data,
                            capture_output=True)
    if result.returncode:
        raise ValueError("firmware signing key or signature is invalid")
    return result.stdout


def publish_firmware(image_path, version, build, key, public, output):
    image_path, output = Path(image_path), Path(output)
    image_data = image_path.read_bytes()
    sha = hashlib.sha256(image_data).hexdigest()
    image_name = f"passport-os-{version}.bin"
    canonical = canonical_manifest(version, build, len(image_data), sha, image_name)
    signature = _openssl(["dgst", "-sha256", "-sign", key], canonical)
    if len(signature) != 256:
        raise ValueError("system release key must be RSA-2048")
    with tempfile.NamedTemporaryFile() as signed:
        signed.write(signature)
        signed.flush()
        _openssl(["dgst", "-sha256", "-verify", public, "-signature",
                  signed.name], canonical)
    output.mkdir(parents=True, exist_ok=True)
    manifest = {
        "format": "passport.firmware/1", "chip": "esp32c3",
        "version": version, "build": build, "size": len(image_data),
        "sha256": sha, "image": image_name,
        "signature": signature.hex(),
    }
    latest = output / "latest.json"
    if latest.exists():
        previous = verify_firmware_manifest(latest.read_bytes(), public)
        if build <= previous['build'] and manifest != previous:
            raise ValueError("release build must increase; existing release is unchanged")
    image_target = output / image_name
    if image_target.exists() and image_target.read_bytes() != image_data:
        raise ValueError("versioned firmware image is immutable; use a new version")
    atomic_write(image_target, image_data)
    # Commit the pointer last. Readers see the entire previous or next manifest.
    atomic_write(latest, (json.dumps(manifest, sort_keys=True, separators=(",", ":")) + "\n").encode())
    return manifest


def verify_firmware_manifest(raw, public, image_data=None):
    def no_duplicates(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError("duplicate manifest field")
            result[key] = value
        return result

    manifest = json.loads(raw, object_pairs_hook=no_duplicates)
    required = {"format", "chip", "version", "build", "size", "sha256",
                "image", "signature"}
    if (not isinstance(manifest, dict) or set(manifest) != required or
            manifest["format"] != "passport.firmware/1" or
            manifest["chip"] != "esp32c3" or
            not isinstance(manifest["version"], str) or
            type(manifest["build"]) is not int or
            type(manifest["size"]) is not int or
            not isinstance(manifest["sha256"], str) or
            not isinstance(manifest["image"], str) or
            not isinstance(manifest["signature"], str) or
            not re.fullmatch(r"[0-9a-f]{512}", manifest["signature"])):
        raise ValueError("unsupported firmware manifest")
    canonical = canonical_manifest(manifest["version"], manifest["build"],
                                   manifest["size"], manifest["sha256"],
                                   manifest["image"])
    signature = bytes.fromhex(manifest["signature"])
    if len(signature) != 256:
        raise ValueError("invalid RSA-2048 signature length")
    with tempfile.NamedTemporaryFile() as signed:
        signed.write(signature)
        signed.flush()
        _openssl(["dgst", "-sha256", "-verify", public, "-signature",
                  signed.name], canonical)
    if image_data is not None and (len(image_data) != manifest["size"] or
                                   hashlib.sha256(image_data).hexdigest() != manifest["sha256"]):
        raise ValueError("firmware image hash or size mismatch")
    return manifest
