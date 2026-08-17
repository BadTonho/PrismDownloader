#!/usr/bin/env python3
"""Create the exact JSON bytes that are signed for a Prism release."""

import argparse
import hashlib
import json
from pathlib import Path


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def parse_asset(value: str) -> tuple[str, Path]:
    name, separator, raw_path = value.partition("=")
    if not separator or not name or not raw_path:
        raise argparse.ArgumentTypeError("asset must be NAME=PATH")
    path = Path(raw_path)
    if not path.is_file():
        raise argparse.ArgumentTypeError(f"asset does not exist: {path}")
    return name, path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--version", required=True)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--asset", required=True, action="append", type=parse_asset)
    args = parser.parse_args()

    expected_names = {
        f"PrismDownloader_v{args.version}_Setup.exe",
        f"PrismDownloader_v{args.version}_Portable.zip",
        f"prism-downloader_{args.version}_amd64.deb",
    }
    assets = dict(args.asset)
    if set(assets) != expected_names or len(assets) != 3:
        raise SystemExit("manifest must contain exactly the Windows Setup, Windows Portable, and Linux DEB assets")

    payload = {
        "assets": [
            {"name": name, "sha256": sha256_file(assets[name])}
            for name in sorted(assets)
        ],
        "schema": 1,
        "version": args.version,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(
        (json.dumps(payload, sort_keys=True, separators=(",", ":")) + "\n").encode("utf-8")
    )


if __name__ == "__main__":
    main()
