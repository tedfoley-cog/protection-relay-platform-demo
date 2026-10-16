/* ANSI 87B low-impedance percentage-restrained busbar differential.
 *
 * Per phase, over the in-zone bays: Id = |sum I_k| / Ib, Ir = sum |I_k| / Ib,
 * operate when Id > pickup and Id > T(Ir) with a dual-slope T continuous at
 * the breakpoint. Phasors are supplied by the caller for one protection pass,
 * in primary amps with current INTO the bus positive. Any invalid channel of
 * an in-zone bay blocks the element for that pass; nothing is substituted.
 * Design: docs/elements/87B/DESIGN_SPEC.md. */
#ifndef BUS_DIFF_87B_H
#define BUS_DIFF_87B_H

#include "cplx.h"
#include "ied_config.h"

#include <stdbool.h>
#include <stdint.h>

#define BUS_DIFF_87B_CTS_MIN_LOAD_PU 0.1f   /* prototype assumption A7 */

typedef struct {
    uint8_t enabled;                   /* 87B_FUNCTION */
    float base_mva;                    /* 87B_BASE_MVA, with VT nominal defines Ib (A1) */
    float pickup_pu;                   /* 87B_PICKUP */
    float slope1_pct;                  /* 87B_SLOPE1 */
    float slope2_pct;                  /* 87B_SLOPE2 (>= slope 1, A3) */
    float breakpoint_pu;               /* 87B_BREAKPOINT */
    float cts_alarm_pct;               /* 87B_CTS_ALARM_PCT */
    float cts_delay_s;                 /* 87B_CTS_DELAY */
    uint8_t bay_in_zone[IED_NUM_BAYS]; /* 87B_BAYn_IN_ZONE */
    /* derived by ied_settings_finalize() */
    float base_a;                      /* Ib, primary A */
    uint32_t cts_delay_passes;
} bus_diff_87b_settings_t;

typedef struct {
    float id_pu[IED_PHASES];
    float ir_pu[IED_PHASES];
    bool pickup[IED_PHASES];
    bool operate[IED_PHASES];
    bool operated;        /* any phase operated this pass and not blocked */
    bool blocked;         /* invalid in-zone input or CTS alarm */
    bool cts_alarm;
    bool enabled;
    uint32_t cts_count;   /* passes the CTS condition has stood */
    uint32_t cts_clear_count;
    uint8_t op_phase;     /* first operating phase, for the SOE value */
} bus_diff_87b_state_t;

void bus_diff_87b_init(bus_diff_87b_state_t *st);

/* Operate threshold T(Ir) in pu for the dual-slope characteristic. */
float bus_diff_87b_threshold_pu(const bus_diff_87b_settings_t *s, float ir_pu);

/* One protection pass. i_into_bus[bay][phase] in primary A (into the bus
 * positive); invalid_mask is the frame's channel mask. Returns st->operated. */
bool bus_diff_87b_step(const bus_diff_87b_settings_t *s, bus_diff_87b_state_t *st,
                       const cplx_t i_into_bus[IED_NUM_BAYS][IED_PHASES], uint32_t invalid_mask);

#endif /* BUS_DIFF_87B_H */
