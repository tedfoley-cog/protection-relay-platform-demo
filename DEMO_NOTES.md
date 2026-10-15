# Demo notes — Pilot 1, 87B busbar differential

Setup
- Open `tedfoley-cog/protection-relay-platform-demo` at `main`: four-bay relay firmware (50/51/21/50BF/81, PMU), shared relay math, setting tool, harness contract. No 87B anywhere — `grep -ri 87B firmware shared tool` returns nothing.
- Start a Devin session on the repo with the `safety-critical-change` skill visible; have `tool/public/realtime.json` (baseline real-time report) and `requirements/customer/87B-customer-requirement.md` open in a second tab.
- Reference answers (atomic spec, planted-problem list, closed-form oracle) sit in the private companion repo; never open it on screen until the comparison step.

Flow
- Intake: ask Devin to decompose `PCE-REQ-2026-087` into atomic `REQ-87B-nnn` rows per `docs/standards/REQUIREMENTS_CONVENTION.md`. Expect it to flag the planted gaps as `BLOCKED` (slope order, pu base vs CT-rating range, bay 4 with no CTs, seal-in drop-out, "block" without a source, disable/power-up/settings-change behaviour, bay 5) rather than pick defaults. Compare live against the private planted-problem list.
- Design + build: Devin fills `DESIGN_SPEC_TEMPLATE.md`, then follows the skill: pin tests, failing test from the `bus_fault_internal` / `bus_fault_external` scenarios, smallest fail-safe element in `firmware/src/prot/bus_diff_87b.c`, rule IDs `R-87B-nnn` in `shared/relay_math`, settings keys, tool fields. Show the failing test before the element exists.
- Validate: `make -C firmware test`, seeded invariant sweep (≥ 500 cases, one bay invalid ⇒ no trip on load), `python3 tools/build_realtime_report.py` — protection task still within 12 000 cycles; compare trip time on `bus_fault_internal` to the oracle's closed-form answer.
- Deliver: draft PR with blast-radius statement and before/after report; CI shows the two-reviewer gate on protected paths; CODEOWNERS blocks merge. Nothing merges to `main`.
- Finale: Devin hands the setting tool to its testing agent — new 87B page, slope-order rule error, write blocked, fix, write accepted — recorded in the browser and attached to the PR.

Single-session fallback prompt
- "Read `harness.yaml`, `docs/HARNESS_CONTRACT.md` and `requirements/customer/87B-customer-requirement.md`. Decompose the requirement into atomic REQ-87B rows (mark every gap BLOCKED with a question), write the design spec from the template, then implement the 87B element following `.agents/skills/safety-critical-change`, validate it with the bus-fault scenarios and the real-time report, add it to the setting tool, and open a draft PR with the blast-radius statement. Do not merge."
