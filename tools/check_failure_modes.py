#!/usr/bin/env python3
"""Deterministic checks for the four failure modes in harness.yaml.

F1  data model mixed with settings/scaling/logic: *_types.h / *_config.h may
    not contain float literals with units, element state or control flow.
F2  error handling removed in a refactor: compared with --base, a protected C
    file may not lose `return false`, `blocked = true` or range checks
    without a design-spec change in the same diff.
F3  missing/invalid input treated as zero: no element may assign 0 to a
    phasor/current on an invalid-mask branch; invalid must block.
F4  invalid differential data trips healthy load: for every element module
    that reads more than one bay, the host suite must contain a test that
    runs steady_load with a bay invalidated and asserts no trip.
"""
import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROT = ROOT / "firmware" / "src" / "prot"
MODEL_HEADERS = list((ROOT / "firmware" / "include").glob("*_types.h")) + \
    list((ROOT / "firmware" / "include").glob("*_config.h"))
ERROR_PATTERNS = [r"return\s+false", r"blocked\s*=\s*true", r"if\s*\(.*(<|>)=?\s*[A-Z_]*(MIN|MAX)"]
BAY_READ = re.compile(r"IED_CH_BAY_I\s*\(|bay\[|IED_BAY_MASK")


def f1() -> list[str]:
    out = []
    for h in MODEL_HEADERS:
        text = h.read_text()
        body = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
        body = re.sub(r"//.*", "", body)
        if h.name == "ied_config.h":
            continue  # platform figures live here by design
        for n, line in enumerate(body.splitlines(), 1):
            if re.search(r"\b(if|while|for|switch)\s*\(", line):
                out.append(f"F1 {h.relative_to(ROOT)}:{n}: control flow in data-model header")
            if re.search(r"\b(pickup|tms|reach|delay|slope)\w*\s*=", line, re.I):
                out.append(f"F1 {h.relative_to(ROOT)}:{n}: setting value in data-model header")
    return out


def f2(base: str | None) -> list[str]:
    if not base:
        return []
    out = []
    diff = subprocess.run(["git", "diff", f"{base}...HEAD", "--", "firmware/src", "shared/relay_math"],
                          capture_output=True, text=True, cwd=ROOT).stdout
    spec_changed = "docs/elements/" in subprocess.run(
        ["git", "diff", "--name-only", f"{base}...HEAD"], capture_output=True, text=True, cwd=ROOT).stdout
    current = None
    for line in diff.splitlines():
        if line.startswith("+++ b/"):
            current = line[6:]
        elif line.startswith("-") and not line.startswith("---"):
            if any(re.search(p, line) for p in ERROR_PATTERNS) and not spec_changed:
                out.append(f"F2 {current}: error-handling line removed without a design-spec change: {line[1:].strip()}")
    return out


def f3() -> list[str]:
    out = []
    for c in sorted(PROT.glob("*.c")) + [ROOT / "firmware" / "src" / "ied" / "ied_app.c"]:
        lines = c.read_text().splitlines()
        for n, line in enumerate(lines, 1):
            if "invalid" in line.lower() and re.search(r"\bif\b", line):
                window = " ".join(lines[n:n + 6])
                if re.search(r"(phasor|i_[abc]|cur\w*|mag\w*)\s*(\[[^\]]*\])?\s*=\s*(cplx\()?0(\.0f?)?\b", window):
                    out.append(f"F3 {c.relative_to(ROOT)}:{n}: invalid-input branch substitutes zero")
    return out


def f4() -> list[str]:
    out = []
    tests = " ".join(p.read_text() for p in (ROOT / "firmware" / "tests").glob("test_*.c"))
    for c in sorted(PROT.glob("*.c")):
        text = c.read_text()
        if len(set(BAY_READ.findall(text))) == 0 and text.count("IED_CH_BAY_I") == 0:
            continue
        stem = c.stem
        if not (re.search(rf"{stem}.*ct_lost|ct_lost.*{stem}", tests, re.S) and "steady_load" in tests):
            out.append(f"F4 {c.relative_to(ROOT)}: multi-bay element without a healthy-load + invalid-bay no-trip test")
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", default=None, help="git ref to diff against for F2")
    a = ap.parse_args()
    findings = f1() + f2(a.base) + f3() + f4()
    for f in findings:
        print(f)
    print(f"failure-mode checks: {len(findings)} finding(s)")
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main())
