# Design specification: <ANSI> <element name>

| | |
|---|---|
| Requirement set | `docs/elements/<ansi>/REQUIREMENTS.md` |
| Safety classification | protection function (see `harness.yaml`) |
| Governing standards | IEC 60255-1, IEC 60255-<part>, IEEE <std> |
| Status | draft / reviewed / implemented |

## 1. Function

One paragraph: what the element protects, what it measures, what it outputs.

## 2. Inputs

| Input | Source | Unit | Validity handling |
|---|---|---|---|
| phasors | `ied_prot_ctx_t` channels ... | primary A | block if `invalid_mask` covers any |
| settings | keys ... | | validated by rules R-... |
| breaker status | | | |

## 3. Settings

| Key | Range | Default | Rule | Requirement |
|---|---|---|---|---|

## 4. Algorithm

Equations with units. State machine (states, transitions, timers in
protection passes). Per-unit base stated explicitly.

## 5. Operands and outputs

| Operand | Meaning | Set when | Reset when |
|---|---|---|---|

## 6. Behaviour in degraded conditions

Invalid input, CT not connected, settings change mid-pickup, power-up,
disable, external block. One row each; each cites a requirement.

## 7. Timing and budget

Expected operate time at 2×/5×/10×, drop-out, seal-in; estimated cycles per
pass against `IED_BUDGET_PROT_CYCLES`.

## 8. Traceability

| Requirement | Section | Test |
|---|---|---|

## 9. Blast radius

Files touched, elements affected, operands added. Statement of what cannot
change (existing scenarios' expected elements and trip times).
