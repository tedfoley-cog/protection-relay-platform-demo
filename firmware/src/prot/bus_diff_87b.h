/* ANSI 87B low-impedance percentage-restrained busbar differential.
 *
 * Per phase, over the in-zone bays: Id = |DFT(sum_k s_k x_k)| / Ib where the
 * polarity-corrected bay samples are summed first and mimic-filtered once
 * (linear filters: identical to summing identically filtered phasors), and
 * Ir = sum_k RMS_k / Ib from a full-cycle sliding sum of squares per bay.
 * Operate when Id > pickup and Id > T(Ir), dual slope continuous at the
 * breakpoint. Any invalid channel of an in-zone bay blocks the element and
 * re-arms it for one full window; nothing is ever substituted for data.
 * Per-sample work is done in bus_diff_87b_ingest() (protection task, from
 * ingest()); the decision in bus_diff_87b_step() once per protection pass.
 * Design: docs/elements/87B/DESIGN_SPEC.md. */
#ifndef BUS_DIFF_87B_H
#define BUS_DIFF_87B_H

#include "cplx.h"
#include "ied_config.h"
#include "phasor.h"

#include <stdbool.h>
#include <stdint.h>

#define BUS_DIFF_87B_CTS_MIN_LOAD_PU 0.1f           /* prototype assumption A7 */
#define BUS_DIFF_87B_WINDOW IED_SAMPLES_PER_CYCLE   /* 32 samples, one cycle */
#define BUS_DIFF_87B_ARM_PASSES (IED_SAMPLES_PER_CYCLE / IED_PROT_DECIMATION)

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
    /* operands and actual values (last pass) */
    float id_pu[IED_PHASES];
    float ir_pu[IED_PHASES];
    bool pickup[IED_PHASES];
    bool operate[IED_PHASES];
    bool operated;        /* any phase operated this pass and not blocked */
    bool blocked;         /* invalid in-zone input, arming, or CTS alarm */
    bool cts_alarm;
    bool enabled;
    uint32_t cts_count;   /* passes the CTS condition has stood */
    uint32_t cts_clear_count;
    uint32_t valid_passes;
    uint8_t op_phase;     /* first operating phase, for the SOE value */
    /* per-sample measurement state */
    float diff[IED_PHASES][BUS_DIFF_87B_WINDOW];            /* mimic-filtered differential samples */
    uint32_t sis[BUS_DIFF_87B_WINDOW];                      /* sample-in-second per slot */
    float sq[IED_NUM_BAYS][IED_PHASES][BUS_DIFF_87B_WINDOW]; /* squared bay samples */
    float sq_sum[IED_NUM_BAYS][IED_PHASES];                 /* sliding sum over the window */
    mimic_filter_t mimic[IED_PHASES];
    uint32_t n;           /* samples ingested */
    uint32_t invalid_seen; /* channel mask OR-ed over the frames ingested since the last step */
    uint32_t anchor;      /* next (bay, phase) whose sq_sum is recomputed exactly */
} bus_diff_87b_state_t;

void bus_diff_87b_init(bus_diff_87b_state_t *st, float x_over_r);

/* Operate threshold T(Ir) in pu for the dual-slope characteristic. */
float bus_diff_87b_threshold_pu(const bus_diff_87b_settings_t *s, float ir_pu);

/* One acquisition sample: raw_a[bay][phase] in primary A as measured (before
 * any mimic), into_bus_sign[bay] = +1 if positive measured current flows
 * into the bus, -1 otherwise. */
void bus_diff_87b_ingest(const bus_diff_87b_settings_t *s, bus_diff_87b_state_t *st,
                         const float raw_a[IED_NUM_BAYS][IED_PHASES],
                         const int8_t into_bus_sign[IED_NUM_BAYS], uint32_t sample_in_second,
                         uint32_t frame_invalid_mask);

/* One protection pass. invalid_mask is the deciding frame's channel mask; it
 * is OR-ed with every mask seen by ingest() since the previous step, so a
 * channel that was invalid for any sample of the pass blocks the pass.
 * Returns st->operated. */
bool bus_diff_87b_step(const bus_diff_87b_settings_t *s, bus_diff_87b_state_t *st,
                       uint32_t invalid_mask);

#endif /* BUS_DIFF_87B_H */
