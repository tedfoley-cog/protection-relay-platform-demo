# Unresolved platform questions

Known gaps a new element may hit. Each is a `BLOCKED` item for the requirement
owner unless the element's design spec resolves it explicitly.

| # | Question | Why it matters | Default if unanswered |
|---|---|---|---|
| Q1 | Is the mimic (DC-offset) filter required on bays 2-4? Today only bay-1 currents are mimic-compensated. | Any element summing bay currents sees unequal DC rejection between bays. | not applied; element must tolerate or add it with budget evidence |
| Q2 | CT saturation model for external faults. The test set has ideal CTs. | Security of any differential or directional scheme is unproven without it. | ideal CTs; state the limitation |
| Q3 | Per-unit base for multi-bay quantities: largest CT primary, bus rating in MVA, or per-bay? | Two valid conventions give different slope numbers. | none; requirement must state it (`pu_base_a_from_mva` and `pu_ct_ratio` both exist) |
| Q4 | Behaviour of an element while `invalid_mask` covers a bay it does not use. | Over-blocking reduces dependability; under-blocking risks misoperation. | block only on channels the element reads |
| Q5 | Settings-change semantics: reset element timers on group write, or hold? | A timed element mid-pickup during a write could trip or stall. | reset all element state on `ied_settings_finalize` |
| Q6 | Power-up: elements are blocked for one cycle until the DFT window fills. Is a longer arming delay required? | Energising onto a fault vs. inrush security. | one cycle |
| Q7 | 50 Hz support is build-time only. | A 50 Hz customer needs a second build and test campaign. | 60 Hz |
| Q8 | Breaker status inputs per bay are not modelled (only bay-1 52a). | Bay-out-of-service handling for bus schemes. | not available; treat as in service |
