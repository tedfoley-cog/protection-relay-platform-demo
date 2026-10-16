/* Relay application: protection elements + synchrophasor unit on the four-bay
 * measurement chain (bay 1 is the relay's own feeder).
 *
 * Execution model (fixed-priority, preemptive, no RTOS):
 *
 *   prio 0  ied_acq_isr()          1920 Hz  ADC frame -> ring, timebase
 *   prio 0  ied_pps_isr()          1 Hz     station clock discipline
 *   prio 1  ied_protection_task()  480 Hz   phasors, 50/51, 21, 50BF, trip matrix
 *   prio 2  ied_pmu_task()         60 Hz    C37.118 P-class phasors, 81U/O/R, framing
 *   thread  ied_comms_task()       120 Hz   DNP3 points, SOE drain
 *
 * Every task records its execution time in cycle_stats against the budgets
 * in ied_config.h. All state is statically allocated in this module.
 */
#ifndef IED_APP_H
#define IED_APP_H

#include "bay_config.h"
#include "breaker_failure.h"
#include "bus_diff_87b.h"
#include "distance.h"
#include "event_recorder.h"
#include "freq_element.h"
#include "ied_config.h"
#include "oc_element.h"
#include "synchrophasor.h"
#include "timebase.h"
#include "trip_matrix.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define IED_PMU_NUM_PHASORS 9u   /* VA VB VC IA IB IC IN V1 I1 */

typedef struct {
    /* scale[] for current channels is derived from bay[] by ied_settings_finalize();
     * only the VT entries are set directly. */
    float scale[IED_NUM_CHANNELS];   /* primary units per ADC count */
    bay_config_t bay[IED_NUM_BAYS];
    float v_ln_nominal;              /* nominal phase-to-neutral voltage, primary V RMS */
    float x_over_r;                  /* source X/R for the current mimic filters */
    ioc_settings_t i50p;
    ioc_settings_t i50g;
    toc_settings_t t51p;
    toc_settings_t t51g;
    distance_settings_t dist;
    bf_settings_t bf;
    freq_settings_t freq;
    bus_diff_87b_settings_t b87;
    trip_matrix_settings_t matrix;
    uint16_t pmu_idcode;
    const char *station;
} ied_settings_t;

typedef struct {
    cplx_t v[3];
    cplx_t i[3];
    float in_a;
    elem_mask_t operated;
    elem_mask_t target;
    bool trip1;
    bool tripb;
    bool breaker_open;
    float freq_hz;
    float rocof_hz_s;
    bool pps_locked;
    uint32_t protection_passes;
    uint32_t pmu_frames;
    uint32_t pmu_overruns;
    uint32_t ring_overruns;
    uint32_t invalid_mask;           /* channels not valid in the last frame */
    uint32_t blocked_passes;         /* protection passes skipped for invalid bay-1 input */
    bus_diff_87b_state_t b87;        /* 87B operands and actual values, last pass */
} ied_status_t;

typedef struct {
    void (*pmu_frame)(const uint8_t *frame, size_t len, const pmu_measurement_t *m,
                      uint32_t soc, uint32_t fracsec, void *ctx);
    void (*event)(const soe_event_t *ev, void *ctx);
    void *ctx;
} ied_hooks_t;

/* Exposed so a test harness can fast-forward relay uptime. */
extern timebase_t g_timebase;

const ied_settings_t *ied_default_settings(void);

/* Derives the current-channel scale from the bay CT ratios. Called by
 * ied_init; exposed for the settings-file writer and tests. */
void ied_settings_finalize(ied_settings_t *s);

void ied_init(const ied_settings_t *s, uint32_t soc_at_boot, const ied_hooks_t *hooks);

/* Returns the timer reload for the next sample period. `invalid_mask` marks
 * channels whose input is not valid (CT/ADC supervision); bays without a CT
 * source are added automatically. Elements fed by an invalid channel hold
 * their state and do not operate — missing data is never read as zero. */
uint32_t ied_acq_isr_q(const int16_t raw[IED_NUM_CHANNELS], uint32_t invalid_mask);
uint32_t ied_acq_isr(const int16_t raw[IED_NUM_CHANNELS]);
void ied_pps_isr(uint32_t counts_since_last_pps);

/* Returns true when a synchrophasor frame has become due. */
bool ied_protection_task(void);
void ied_pmu_task(void);
void ied_comms_task(void);

/* 52b breaker auxiliary contact. */
void ied_set_breaker_open(bool open);
void ied_reset_lockout(void);

const ied_status_t *ied_status(void);
size_t ied_pmu_config_frame(uint8_t *buf, size_t len);
size_t ied_dnp3_class0(uint8_t *buf, size_t len);

#endif /* IED_APP_H */
