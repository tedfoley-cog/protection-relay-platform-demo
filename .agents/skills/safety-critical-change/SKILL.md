---
name: safety-critical-change
description: Mandatory procedure for any change to protection firmware in this repo — protection elements (firmware/src/prot), trip logic and settings path (firmware/src/ied), the measurement chain (firmware/src/rt, firmware/src/dsp, firmware/include) and the shared relay math (shared/relay_math). Pins current behaviour, reproduces the defect or states the new requirement with a failing test, applies the smallest fail-safe change, proves a seeded safety invariant, re-runs the real-time report and opens a draft PR with the evidence. Invoke before editing any of those paths, including new elements, bug fixes, refactors and ports.
---

# Safety-critical change

This firmware trips breakers. A wrong answer either leaves a bus fault on the
system or de-energises healthy feeders, so the bar is evidence, not
plausibility. Do the steps in order and do not edit production code before
step 3. `harness.yaml` lists the paths this applies to (`protected_paths`).

## 1. Baseline

```bash
make -C firmware test
make -C shared/relay_math all
python3 tools/build_realtime_report.py   # writes tool/public/realtime.json
cp tool/public/realtime.json /tmp/realtime-before.json
```

Everything must be green and every task within budget before you start.
Keep `realtime-before.json`; the PR needs it.

## 2. Pin current behaviour

Add a `firmware/tests/test_*.c` characterization test that pins what the
touched module does **today** on inputs it already handles: every existing
scenario's expected elements and trip time (`tools/build_realtime_report.py`
`EXPECTED`), every operand, not just one. These tests must pass on the
unmodified code and must not change in step 4. If one has to change, stop
and explain why in the PR.

## 3. Reproduce or specify

* Defect: write the smallest test that fails on current code and states the
  defect in protection terms ("51P operates on `ct_lost_bay1_load`"). Keep
  the failure output. If you cannot make it fail, the defect is not
  confirmed — report and stop.
* New element or behaviour: write the failing test from the requirement row
  (`REQ-xx-nnn` in the test name or a comment) before the element exists.
  Use the secondary-injection test set (`firmware/testset`) and add a
  scenario rather than mocking phasors.
* Refactor or port: no defect to reproduce; the step-2 tests are the gate.

## 4. Smallest fail-safe change

Change the least code that makes step 3 pass while step 2 stays green.
Fail-safe means: on any doubt (invalid input, CT not connected, settings not
finalized, window not full) the element **blocks** — it does not trip, and it
does not substitute zero or a stale value. `docs/standards/CODING_STANDARD.md`
rules 3 and 4 apply; `python3 tools/check_failure_modes.py --base origin/main`
must report zero findings.

## 5. Invariant sweep

Add or extend a seeded sweep of at least 500 generated cases
(`srand(fixed_seed)`, print the seed) over the element's input space and
assert the safety property, for example:

* no trip on any healthy-load case, including cases with one bay invalid;
* trip on every internal-fault case above the stated margin;
* operate time within the design-spec tolerance of the shared-math value.

Print the case that fails, with its seed index, so it can be replayed.

## 6. Target evidence

```bash
make -C firmware/target/stm32f407
python3 tools/run_renode.py --all
python3 tools/build_realtime_report.py
python3 tools/check_traceability.py
```

The protection task must stay within `IED_BUDGET_PROT_CYCLES` on every
scenario. Compare `tool/public/realtime.json` with `/tmp/realtime-before.json`:
every pre-existing scenario keeps its expected elements and its `trip_ms`
within ±1 protection pass (2.1 ms).

## 7. Draft PR

Open a **draft** PR (never push to `main`). The description contains:

1. the requirement rows addressed, with any `BLOCKED` rows left open;
2. the step-3 failure output and the step-5 sweep summary (seed, cases);
3. before/after real-time report table (tasks, worst scenario, headroom);
4. a **blast-radius statement**: files touched, elements whose operands
   changed, scenarios whose timing moved, what cannot have changed and why;
5. the output of `tools/check_failure_modes.py` and
   `tools/check_protected_paths.py --base origin/main`.

Protected paths need two human approvals through CODEOWNERS; do not
self-approve, do not merge, do not mark ready-for-review until the
requirement owner has answered every `BLOCKED` row that the change depends on.
