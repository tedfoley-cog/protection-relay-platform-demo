# Requirements convention

## IDs

`REQ-<ANSI>-<NNN>` for functional requirements of an element
(`REQ-51-003`), `REQ-PLT-<NNN>` for platform requirements, `REQ-SAF-<NNN>`
for safety requirements. IDs are stable forever; a withdrawn requirement keeps
its number with status `withdrawn`. Rule IDs for settings validation are
`R-<ANSI>-<NNN>` and map to one or more REQ IDs.

## Atomic requirement

One testable statement, one ID. Shall / shall not. Includes:

| Field | Content |
|---|---|
| ID | `REQ-87B-004` |
| Statement | "The element shall ..." with numbers and units |
| Rationale | one sentence |
| Source | customer requirement section, standard clause, or platform question |
| Acceptance | measurable check, the scenario or test that proves it |
| Verification | `test` / `analysis` / `inspection` / `demonstration` |
| Status | `draft` / `agreed` / `implemented` / `verified` / `withdrawn` / `BLOCKED` |

## Decomposing a customer requirement

1. Number every sentence of the source; nothing is dropped silently.
2. Split compound sentences; each atomic statement gets an ID.
3. Anything ambiguous, contradictory or missing becomes a `BLOCKED` row with
   the question written for the requirement owner. **Do not resolve it by
   choosing a default** unless `docs/platform/OPEN_QUESTIONS.md` names one,
   in which case cite the default and keep the row `BLOCKED`.
4. Add platform-implied requirements (invalid input, settings change,
   power-up, CPU budget) even if the customer did not mention them.
5. Trace: requirement → design-spec section → test name → manual chapter.

## Traceability in code

Tests reference IDs in their name or a comment: `/* REQ-51-003 */`. The CI
check `tools/check_traceability.py` lists requirement IDs that no test
references.
