#!/usr/bin/env python3
"""Two-human-reviewer gate for protected paths (harness.yaml: protected_paths).

Usage: check_protected_paths.py --base <ref> [--approvals N] [--head <ref>]
Exits 1 when the diff touches a protected path and fewer than two approvals
are reported (CI passes the count from the PR review API; locally it prints
which paths need the gate).
"""
import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def protected_prefixes() -> dict[str, list[str]]:
    groups: dict[str, list[str]] = {}
    current = None
    for line in (ROOT / "harness.yaml").read_text().splitlines():
        if line.startswith("protected_paths:"):
            current = "on"
            continue
        if current == "on":
            if line and not line.startswith(" "):
                break
            if ":" in line:
                name, rest = line.strip().split(":", 1)
                paths = rest.strip().strip("[]").split(",")
                groups[name] = [p.strip() for p in paths if p.strip()]
    return groups


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", required=True)
    ap.add_argument("--head", default="HEAD")
    ap.add_argument("--approvals", type=int, default=None)
    a = ap.parse_args()
    changed = subprocess.run(["git", "diff", "--name-only", f"{a.base}...{a.head}"],
                             capture_output=True, text=True, cwd=ROOT, check=True).stdout.split()
    hits: dict[str, list[str]] = {}
    for group, prefixes in protected_prefixes().items():
        for f in changed:
            if any(f.startswith(p) for p in prefixes):
                hits.setdefault(group, []).append(f)
    if not hits:
        print("protected paths: none touched")
        return 0
    for g, files in hits.items():
        print(f"protected [{g}]: " + ", ".join(sorted(set(files))))
    if a.approvals is None:
        print("two human approvals required before merge (CODEOWNERS)")
        return 0
    if a.approvals < 2:
        print(f"FAIL: {a.approvals} approval(s), 2 required for protected paths")
        return 1
    print(f"ok: {a.approvals} approvals")
    return 0


if __name__ == "__main__":
    sys.exit(main())
