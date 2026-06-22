#!/usr/bin/env python3
"""
patch_signing.py

Remove signing configuration from build-profile.json5 to build
unsigned test HAP when signing files are unavailable.

Usage:
    python3 patch_signing.py <build-profile.json5-path>
"""
import sys


def patch(filepath: str) -> None:
    with open(filepath, "r", encoding="utf-8") as f:
        lines = f.readlines()

    out: list[str] = []
    in_signing_configs = False
    bracket_depth = 0

    for line in lines:
        stripped = line.strip()

        # Skip entire signingConfigs array content, replace with empty array
        if '"signingConfigs"' in stripped and "[" in stripped:
            in_signing_configs = True
            bracket_depth = stripped.count("[") - stripped.count("]")
            out.append('    "signingConfigs": [],\n')
            if bracket_depth <= 0:
                in_signing_configs = False
            continue

        if in_signing_configs:
            bracket_depth += stripped.count("[") - stripped.count("]")
            if bracket_depth <= 0:
                in_signing_configs = False
            continue

        # Remove "signingConfig": "xxx" lines in products
        if '"signingConfig"' in stripped:
            continue

        out.append(line)

    with open(filepath, "w", encoding="utf-8") as f:
        f.writelines(out)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 patch_signing.py <build-profile.json5>", file=sys.stderr)
        sys.exit(1)
    patch(sys.argv[1])
