#!/usr/bin/env python3
"""Set project(central_logger VERSION ...) from VERSION env (release tags)."""
from __future__ import annotations

import os
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
CMAKE = ROOT / "CMakeLists.txt"


def main() -> int:
    version = os.environ.get("VERSION", "").strip()
    if not version:
        print("VERSION env not set", file=sys.stderr)
        return 1
    text = CMAKE.read_text(encoding="utf-8")
    new, n = re.subn(
        r"(?m)^(project\(central_logger VERSION )[0-9.]+",
        rf"\g<1>{version}",
        text,
        count=1,
    )
    if n != 1:
        print("project(VERSION) line not found in CMakeLists.txt", file=sys.stderr)
        return 1
    CMAKE.write_text(new, encoding="utf-8")
    print(f"CMakeLists.txt → VERSION {version}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
