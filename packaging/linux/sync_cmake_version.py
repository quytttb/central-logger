#!/usr/bin/env python3
"""Backward-compatible wrapper: VERSION env -> bump_version.py set."""
from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

_HERE = Path(__file__).resolve().parent
_BUMP = _HERE / "bump_version.py"


def main() -> int:
    version = os.environ.get("VERSION", "").strip()
    if not version:
        print("VERSION env not set", file=sys.stderr)
        return 1
    return subprocess.call([sys.executable, str(_BUMP), "set", version])


if __name__ == "__main__":
    raise SystemExit(main())
