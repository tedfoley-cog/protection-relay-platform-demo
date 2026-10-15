/* Phase and ground overcurrent elements for the real-time core.
 *
 * 50: instantaneous, with a consecutive-pass security count.
 * 51: integrating inverse-time element. Each protection pass adds dt / t(M)
 *     to an accumulator and operates at 1.0, which reproduces the curve
 *     exactly for constant current and behaves correctly when the current
 *     changes during the timing interval (IEC 60255-151 clause 5.3).
 *     IEEE curves use the electromechanical-disc reset of IEEE C37.112;
 *     IEC curves reset instantaneously. The curve math itself lives in
 *     shared/relay_math/toc_curve.c, shared with the setting tool.
 */
#ifndef OC_ELEMENT_H
#define OC_ELEMENT_H

#include <stdbool.h>
#include <stdint.h>

#include "toc_curve.h"

typedef struct {
    float pickup_a;     /* Is, primary amps */
    float tms;          /* time multiplier (IEC) / time dial (IEEE) */
    toc_curve_t curve;
    float definite_s;   /* used when curve == TOC_DEFINITE */
} toc_settings_t;

typedef struct {
    float accum;        /* fraction of the operate time elapsed, 0..1 */
    bool picked_up;
    bool operated;
} toc_state_t;

typedef struct {
    float pickup_a;
    uint8_t security_passes;  /* consecutive passes above pickup before operating */
} ioc_settings_t;

typedef struct {
    uint8_t count;
    bool operated;
} ioc_state_t;


/* Operate time for a constant multiple M = I / Is; negative when M <= 1. */
float toc_operate_time(const toc_settings_t *s, float multiple);

/* Disc reset time for M < 1 (IEEE curves); 0 for instantaneous-reset curves. */
float toc_reset_time(const toc_settings_t *s, float multiple);

void toc_init(toc_state_t *st);
bool toc_step(const toc_settings_t *s, toc_state_t *st, float i_mag, float dt_s);

void ioc_init(ioc_state_t *st);
bool ioc_step(const ioc_settings_t *s, ioc_state_t *st, float i_mag);

#endif /* OC_ELEMENT_H */
