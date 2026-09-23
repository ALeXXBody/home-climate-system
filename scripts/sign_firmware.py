#!/usr/bin/env python3
"""Ed25519 firmware signing for Home Climate System OTA.

Releases should ship each ``.bin`` alongside an Ed25519 signature so the board
can reject tampered images. This script manages the key and signs/verifies.

Usage:
    python3 scripts/sign_firmware.py gen-key          # -> ota_secret.key + ota_public.pem
    python3 scripts/sign_firmware.py sign <file.bin>  # -> <file.bin>.sig
    python3 scripts/sign_firmware.py verify <file.bin> <file.bin.sig> <ota_public.pem>

The private key (``ota_secret.key``) never leaves the build machine. The public
key is baked into the firmware so the board can verify signatures on device.
"""

from __future__ import annotations

import sys
from pathlib import Path

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric.ed25519 import (
    Ed25519PrivateKey,
    Ed25519PublicKey,
)
from cryptography.exceptions import InvalidSignature


def gen_key() -> None:
    sk = Ed25519PrivateKey.generate()
    Path("ota_secret.key").write_bytes(
        sk.private_bytes(
            serialization.Encoding.PEM,
            serialization.PrivateFormat.PKCS8,
            serialization.NoEncryption(),
        )
    )
    Path("ota_public.pem").write_bytes(
        sk.public_key().public_bytes(
            serialization.Encoding.PEM,
            serialization.PublicFormat.SubjectPublicKeyInfo,
        )
    )
    print("wrote ota_secret.key (KEEP PRIVATE) and ota_public.pem")


def _load_key(path: str) -> Ed25519PrivateKey:
    return serialization.load_pem_private_key(
        Path(path).read_bytes(), password=None
    )


def sign(bin_path: str) -> None:
    sk = _load_key("ota_secret.key")
    data = Path(bin_path).read_bytes()
    Path(bin_path + ".sig").write_bytes(sk.sign(data))
    print(f"signed {bin_path} -> {bin_path}.sig")


def verify(bin_path: str, sig_path: str, pub_path: str) -> None:
    pk = serialization.load_pem_public_key(Path(pub_path).read_bytes())
    data = Path(bin_path).read_bytes()
    sig = Path(sig_path).read_bytes()
    try:
        pk.verify(sig, data)
        print("OK — signature valid")
    except InvalidSignature:
        print("FAIL — signature invalid")
        sys.exit(1)


def main() -> None:
    cmd = sys.argv[1] if len(sys.argv) > 1 else ""
    if cmd == "gen-key":
        gen_key()
    elif cmd == "sign" and len(sys.argv) == 3:
        sign(sys.argv[2])
    elif cmd == "verify" and len(sys.argv) == 5:
        verify(sys.argv[2], sys.argv[3], sys.argv[4])
    else:
        print(__doc__)
        sys.exit(2)


if __name__ == "__main__":
    main()
