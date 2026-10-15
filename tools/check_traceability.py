#!/usr/bin/env python3
"""List requirement IDs in docs/elements/*/REQUIREMENTS.md that no test,
scenario or shared-rule references. Exit 1 if any verified requirement is
unreferenced."""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REQ = re.compile(r"^\|\s*(REQ-[A-Z0-9]+-\d{3})\s*\|.*\|\s*(\w+)\s*\|\s*$")


def main() -> int:
    corpus = ""
    for pattern in ("firmware/tests/*.c", "firmware/testset/*.c", "shared/relay_math/*.c",
                    "tools/*.py", "tool/tests/*.ts", "docs/elements/*/VALIDATION_PLAN.md"):
        for p in ROOT.glob(pattern):
            corpus += p.read_text()
    missing = []
    total = 0
    for req_file in ROOT.glob("docs/elements/*/REQUIREMENTS.md"):
        for line in req_file.read_text().splitlines():
            m = REQ.match(line)
            if not m:
                continue
            total += 1
            rid, status = m.groups()
            if status in ("verified", "implemented") and rid not in corpus:
                missing.append(f"{req_file.relative_to(ROOT)}: {rid} ({status}) has no test/plan reference")
    for m in missing:
        print(m)
    print(f"traceability: {total} requirement(s), {len(missing)} unreferenced")
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
