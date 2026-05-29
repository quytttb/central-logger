#!/usr/bin/env python3
"""SemVer bump for project(central_logger VERSION ...) in CMakeLists.txt."""

from __future__ import annotations

import argparse
import os
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
CMAKE = ROOT / "CMakeLists.txt"
_VERSION_RE = re.compile(r"(?m)^(project\(central_logger VERSION )([0-9.]+)")


def _parse_version(raw: str) -> tuple[int, int, int]:
    parts = raw.strip().split(".")
    if len(parts) != 3 or not all(p.isdigit() for p in parts):
        raise ValueError(f"expected X.Y.Z with non-negative integers, got {raw!r}")
    return int(parts[0]), int(parts[1]), int(parts[2])


def read_version() -> str:
    text = CMAKE.read_text(encoding="utf-8")
    m = _VERSION_RE.search(text)
    if not m:
        raise RuntimeError("project(central_logger VERSION ...) not found in CMakeLists.txt")
    return m.group(2)


def write_version(version: str) -> None:
    _parse_version(version)
    text = CMAKE.read_text(encoding="utf-8")
    new_text, count = _VERSION_RE.subn(rf"\g<1>{version}", text, count=1)
    if count != 1:
        raise RuntimeError("could not update VERSION in CMakeLists.txt")
    CMAKE.write_text(new_text, encoding="utf-8")


def bump(level: str) -> str:
    major, minor, patch = _parse_version(read_version())
    if level == "major":
        major += 1
        minor = 0
        patch = 0
    elif level == "minor":
        minor += 1
        patch = 0
    elif level == "patch":
        patch += 1
    else:
        raise ValueError(f"unknown bump level: {level!r}")
    new_version = f"{major}.{minor}.{patch}"
    write_version(new_version)
    return new_version


def main() -> int:
    parser = argparse.ArgumentParser(description="SemVer for central_logger (CMakeLists.txt)")
    sub = parser.add_subparsers(dest="command", required=True)

    sub.add_parser("show", help="print current VERSION")

    bump_p = sub.add_parser("bump", help="bump VERSION in CMakeLists.txt")
    bump_p.add_argument("level", choices=("major", "minor", "patch"))

    set_p = sub.add_parser("set", help="set exact VERSION (CI / tag sync)")
    set_p.add_argument("version", nargs="?", help="X.Y.Z (or VERSION env)")

    args = parser.parse_args()
    try:
        if args.command == "show":
            print(read_version())
            return 0
        if args.command == "bump":
            print(bump(args.level))
            return 0
        version = (args.version or os.environ.get("VERSION", "")).strip()
        if not version:
            print("set requires version argument or VERSION env", file=sys.stderr)
            return 1
        write_version(version)
        print(f"CMakeLists.txt → VERSION {version}")
        return 0
    except (ValueError, RuntimeError) as exc:
        print(exc, file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
