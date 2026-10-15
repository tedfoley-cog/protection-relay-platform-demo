# Measurement chain

```
 CT/VT secondaries ─► anti-alias ─► 16-ch ADC @ 1920 Hz ─► sample_ring (invalid_mask, SIS)
        │                                                        │
        │  one timer, one timestamp per 16-channel frame          ▼
        │                                     every 4th frame: protection pass (480 Hz)
        │                                        ├─ mimic filter (bay-1 currents, X/R setting)
        │                                        ├─ full-cycle DFT per channel ─► phasor[16]
        │                                        ├─ per-bay primary scaling (CT ratio per bay)
        │                                        ├─ elements (50/51/21/50BF/81) ─► operands
        │                                        └─ trip logic ─► outputs, SOE
        └───────────────────────────────────────► PMU task (60 Hz, VA VB VC + bay-1 I)
```

## Channels (`ied_config.h`)

| Index | Channel | Source | Scaling |
|---|---|---|---|
| 0-2 | VA VB VC | bus VTs | `VT_V_LN_NOMINAL` / rated counts |
| 3-5 | B1 IA IB IC | bay-1 CTs | `BAY1_CT_PRIMARY_A` / 1200 counts |
| 6-8 | B2 IA IB IC | bay-2 CTs | `BAY2_CT_PRIMARY_A` / 1200 counts |
| 9-11 | B3 IA IB IC | bay-3 CTs | `BAY3_CT_PRIMARY_A` / 1200 counts |
| 12-14 | B4 IA IB IC | bay-4 CTs | `BAY4_CT_PRIMARY_A` / 1200 counts |
| 15 | IN | bay-1 residual CT | `BAY1_CT_PRIMARY_A` / 1200 counts |

`IED_CH_BAY_I(bay, ph)` returns the channel index; `IED_BAY_MASK(bay)` the
three-channel bit mask used with `invalid_mask`.

## Per-bay configuration (`bay_config.h`)

Each bay carries name, role, CT primary/secondary, polarity and `ct_connected`.
Polarity is the sign convention for current flowing *into the bus*. A bay with
`ct_connected = 0` has no measurement source; its channels are flagged invalid
and any element that needs them must block (see failure-mode checks).

## Time reference

All 16 channels are sampled in the same frame; a frame carries one
second-of-century / tick pair. Elements comparing bays never need alignment
logic, but they must take all phasors from the same protection pass
(`ied_prot_ctx_t`), never from cached copies.

## Validity

`sample_ring.invalid_mask` is set per channel by acquisition (ADC fault, CT
not connected, out-of-range). A pass with any invalid bay-1 channel blocks the
bay-1 elements and increments `blocked_passes`; SOE records `INPUT_INVALID` /
`INPUT_VALID` edges. The rule for new elements: **invalid is not zero**.

## Test set (`firmware/testset/`)

Secondary-injection scenarios drive all 16 channels from an analytic phasor
model with CT ratio, polarity and DC offset per bay. `fault_zone` selects
`ZONE_BAY1_LINE`, `ZONE_BUS` or `ZONE_BAY3_LINE`; `external_clear_s` clears a
fault without opening bay 1; `ct_lost_bay` drops a bay's CTs mid-scenario.
`bus_fault_external` and `bus_fault_internal` exist so that a bus element can
be validated for security and dependability from day one; no element in
`main` uses them yet.
