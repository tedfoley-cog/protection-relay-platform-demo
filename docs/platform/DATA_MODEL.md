# Data model and operands

## Layers (do not mix)

| Layer | Owns | Must not contain |
|---|---|---|
| `relay_types.h`, `ied_config.h` | fixed types, channel indices, rates | settings, scaling, logic |
| `bay_config.h`, `ied_settings_t` | settings values and ranges | computed quantities, element state |
| `pu_scale.h`, `ied_app.c` scaling | counts → primary A / V, per-unit | element decisions |
| `firmware/src/prot/*` | element state machines, operands | I/O, settings parsing |
| `ied_app.c` trip logic | operand → trip matrix, SOE | element math |

`tools/check_failure_modes.py` rejects a change that puts scaling constants or
element logic into a `*_types.h` / `*_config.h` header.

## Operands

Every element exposes boolean operands named `<ANSI><suffix>_<state>` and
numeric operands named `<ANSI>_<quantity>`:

| Operand | Type | Meaning |
|---|---|---|
| `51P_PKP`, `51P_OP` | bool | picked up / operated |
| `51P_T_REM` | float s | time to operate at present current |
| `21Z1_OP`, `21Z2_PKP`, `21Z2_OP` | bool | |
| `50BF_RT`, `50BF_86B` | bool | re-trip, bus trip |
| `81U_OP`, `81O_OP`, `81R_OP` | bool | |
| `INPUT_INVALID` | bitmask | channels flagged invalid this pass |

Operands are written once per protection pass, after all elements ran, from
the element's `*_status_t`. Trip logic reads operands only.

## Settings

Flat `KEY = VALUE` groups (`settings/relay.set`). One group is active; a write
replaces the whole group atomically after `settings_file_check` passes. Keys
follow `<ANSI>_<NAME>_<UNIT>` (`51P_PICKUP_A`, `21_Z2_DELAY_S`) and per-bay
keys `BAY<n>_<NAME>`.

## IEC 61850 mapping (informative)

| Element | Logical node | Data objects used |
|---|---|---|
| 50/51 | `PTOC` | `Str`, `Op`, `StrVal`, `TmMult`, `TmACrv` |
| 21 | `PDIS` | `Str`, `Op`, `PoRch`, `OpDlTmms` |
| 50BF | `RBRF` | `OpEx`, `OpIn`, `FailTmms` |
| 81 | `PTOF`, `PTUF`, `PFRC` | `Str`, `Op`, `StrVal` |
| trip | `PTRC` | `Tr`, `Op` |
| measurement | `MMXU` (bay 1), `MMXN` (bays 2-4) | `A.phsA..C`, `PhV` |

A new element adds a row here and in `docs/standards/INTERFACE_CONTRACTS.md`.
