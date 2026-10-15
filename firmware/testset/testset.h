/* Secondary-injection test set.
 *
 * Generates every analog channel the relay sees from the bus VTs and the CTs
 * of a four-bay 12.47 kV bus section:
 *
 *   bay 1  the relay's own feeder (5 km 336 ACSR, 320 A load)   CT 1200:5, polarity line
 *   bay 2  transformer infeed (20 MVA Thevenin source)           CT 2000:5, polarity bus
 *   bay 3  second feeder (180 A load, same line constants)       CT  600:5, polarity bus
 *   bay 4  third feeder (240 A load)                             CT  800:5, polarity bus
 *
 * Faults are solved with the symmetrical-component networks (SLG, LL, 3PH)
 * on the bay-1 line, on the bay-3 line (external to the bus zone) or on the
 * bus itself (internal). Bay currents always satisfy Kirchhoff at the bus:
 * the sum of currents into the bus is zero for every external condition and
 * equals the fault current for a bus fault. Fault current includes the
 * decaying DC offset set by the point on wave of inception and the X/R of
 * the fault loop. Breaker models respond to the relay's trip outputs; a
 * bus-fault scenario stays uncleared because no element on main protects
 * the bus. All channels share one sample clock.
 *
 * The same code runs on the host (sim/) and inside the target firmware under
 * Renode, standing in for the ADC DMA buffer, so both see identical samples.
 */
#ifndef TESTSET_H
#define TESTSET_H

#include "bay_config.h"
#include "cplx.h"
#include "ied_config.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    FAULT_NONE = 0,
    FAULT_AG,
    FAULT_BG,
    FAULT_CG,
    FAULT_AB,
    FAULT_BC,
    FAULT_CA,
    FAULT_ABC
} fault_type_t;

typedef enum {
    ZONE_BAY1_LINE = 0,   /* fault on the relay's own feeder (default) */
    ZONE_BUS,             /* fault on the protected bus: internal to the bus zone */
    ZONE_BAY3_LINE        /* fault on another feeder: external to the bus zone */
} fault_zone_t;

typedef struct {
    const char *name;
    const char *description;
    float duration_s;
    float freq_hz;            /* system frequency at t = 0 */
    float rocof_hz_s;         /* ramp rate applied from ramp_start_s */
    float ramp_start_s;
    float freq_end_hz;        /* ramp stops here */
    float load_a;
    float load_pf;
    fault_type_t fault;
    float fault_start_s;
    float fault_location_pct; /* % of line length from the relay */
    float fault_r_ohm;
    bool breaker_stuck;
    fault_zone_t fault_zone;
    float external_clear_s;   /* bay-3 fault cleared by its own relay after this long (0: default) */
    uint8_t ct_lost_bay;      /* 1..4: that bay's CT inputs read zero and are flagged invalid */
} scenario_t;

typedef struct {
    const scenario_t *sc;
    float scale[IED_NUM_CHANNELS];
    uint32_t n;               /* samples generated */
    uint32_t preroll_n;       /* steady-state samples before scenario time 0 */
    uint32_t phase_acc;       /* DDS phase of the system voltage, 2^32 = 2 pi */
    float freq_hz;

    cplx_t v_pre[3];
    cplx_t i_pre[IED_NUM_BAYS][3];    /* current out of the bus into each bay */
    cplx_t v_flt[3];
    cplx_t i_flt[IED_NUM_BAYS][3];
    cplx_t z_load[IED_NUM_BAYS];      /* load impedance of each feeder bay (bay 2: 0) */
    float fault_tau_samples;
    float dc_offset[IED_NUM_BAYS][3]; /* current step at inception, per bay and phase */
    int8_t polarity[IED_NUM_BAYS];
    uint32_t fault_n;
    bool fault_active;

    bool breaker_open;        /* bay-1 breaker */
    bool bus_dead;
    bool external_cleared;
    int32_t breaker_timer;    /* samples until the feeder breaker opens */
    int32_t bus_timer;
    uint32_t lcg;
} testset_t;

#define TESTSET_BREAKER_SAMPLES   96   /* 3-cycle breaker: 50 ms */
#define TESTSET_BUS_SAMPLES       154  /* 5-cycle upstream breakers: 80 ms */

int testset_scenario_count(void);
const scenario_t *testset_scenario(int index);
const scenario_t *testset_find(const char *name);

void testset_init(testset_t *ts, const scenario_t *sc, const float scale[IED_NUM_CHANNELS]);

/* Default bay table matching the scenarios above (same object the relay's
 * default settings use). */
const bay_config_t *testset_default_bays(void);

/* Produce the next simultaneous sample. trip1/tripb are the IED outputs from
 * the previous sample period. */
void testset_step(testset_t *ts, bool trip1, bool tripb, int16_t raw[IED_NUM_CHANNELS]);

/* Channel bits whose inputs are not valid for the current sample (lost CT). */
uint32_t testset_invalid_mask(const testset_t *ts);

/* Steady-state phasors of the current sample period: current out of the bus
 * into `bay` (primary A RMS, before CT polarity), for tests. */
cplx_t testset_bay_current(const testset_t *ts, uint32_t bay, int phase);

/* Hold the pre-fault steady state for `samples` before scenario time 0 (the
 * IED needs three PPS edges to declare time lock). */
void testset_set_preroll(testset_t *ts, uint32_t samples);

bool testset_done(const testset_t *ts);
float testset_time_s(const testset_t *ts);

/* Steady-state phasor solution, exposed for tests (RMS, primary units). */
void testset_solve_fault(fault_type_t type, float location_pct, float rf_ohm, cplx_t v[3],
                         cplx_t i[3], float *x_over_r);

#endif /* TESTSET_H */
