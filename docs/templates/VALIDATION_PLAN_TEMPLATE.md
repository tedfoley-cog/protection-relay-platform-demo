# Validation plan: <ANSI> <element name>

## 1. Scope and pass criteria

Element, firmware version, settings group used. The plan passes when every
row in §3 passes on host **and** on the Renode target, the real-time report
shows the protection task within budget on all scenarios, and no pre-existing
scenario changes its expected elements or trip time by more than ±1 pass.

## 2. Test levels

| Level | Tool | What it proves |
|---|---|---|
| Unit | `firmware/tests/test_<ansi>.c` | equations, state machine, rules |
| Invariant | seeded sweep ≥ 500 cases | no trip on any healthy-load case; trip on every internal-fault case above margin |
| Scenario (host) | `firmware/sim/ied_sim` + test set | end-to-end with SOE |
| Scenario (target) | `tools/run_renode.py` | same on STM32F407, cycle counts |
| Tool | Playwright | settings entry, rule errors, write |
| Parity | `test_shared_parity` | firmware and tool math agree on vectors |

## 3. Test matrix

| ID | Requirement | Condition | Expected | Level | Result |
|---|---|---|---|---|---|

## 4. Performance targets

Filled copy of `docs/platform/PERFORMANCE_TARGETS.md`.

## 5. Evidence

`realtime.json` before/after, test logs, Playwright report, blast-radius
statement, reviewer sign-off (two human reviewers for protection paths).
