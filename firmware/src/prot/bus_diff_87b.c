#include "bus_diff_87b.h"

#include <math.h>

static void clear_quantities(bus_diff_87b_state_t *st)
{
    for (uint32_t ph = 0; ph < IED_PHASES; ph++) {
        st->id_pu[ph] = 0.0f;
        st->ir_pu[ph] = 0.0f;
        st->pickup[ph] = false;
        st->operate[ph] = false;
    }
    st->operated = false;
}

static void reset_logic(bus_diff_87b_state_t *st)
{
    clear_quantities(st);
    st->blocked = false;
    st->cts_alarm = false;
    st->cts_count = 0u;
    st->cts_clear_count = 0u;
    st->valid_passes = 0u;
    st->op_phase = 0u;
}

void bus_diff_87b_init(bus_diff_87b_state_t *st, float x_over_r)
{
    phasor_tables_init();   /* phasor_ref() needs the tables; idempotent */
    reset_logic(st);
    st->enabled = false;
    for (uint32_t ph = 0; ph < IED_PHASES; ph++) {
        mimic_init(&st->mimic[ph], x_over_r);
        for (uint32_t m = 0; m < BUS_DIFF_87B_WINDOW; m++) {
            st->diff[ph][m] = 0.0f;
        }
    }
    for (uint32_t m = 0; m < BUS_DIFF_87B_WINDOW; m++) {
        st->sis[m] = 0u;
    }
    for (uint32_t bay = 0; bay < IED_NUM_BAYS; bay++) {
        for (uint32_t ph = 0; ph < IED_PHASES; ph++) {
            st->sq_sum[bay][ph] = 0.0f;
            for (uint32_t m = 0; m < BUS_DIFF_87B_WINDOW; m++) {
                st->sq[bay][ph][m] = 0.0f;
            }
        }
    }
    st->n = 0u;
    st->anchor = 0u;
}

float bus_diff_87b_threshold_pu(const bus_diff_87b_settings_t *s, float ir_pu)
{
    float s1 = s->slope1_pct * 0.01f;
    float s2 = s->slope2_pct * 0.01f;
    float bp = s->breakpoint_pu;
    if (ir_pu <= bp) {
        return s1 * ir_pu;
    }
    return s1 * bp + s2 * (ir_pu - bp);
}

void bus_diff_87b_ingest(const bus_diff_87b_settings_t *s, bus_diff_87b_state_t *st,
                         const float raw_a[IED_NUM_BAYS][IED_PHASES],
                         const int8_t into_bus_sign[IED_NUM_BAYS], uint32_t sample_in_second)
{
    uint32_t slot = st->n & (BUS_DIFF_87B_WINDOW - 1u);
    st->sis[slot] = sample_in_second;
    for (uint32_t ph = 0; ph < IED_PHASES; ph++) {
        float d = 0.0f;
        for (uint32_t bay = 0; bay < IED_NUM_BAYS; bay++) {
            if (s->bay_in_zone[bay] == 0u) {
                continue;
            }
            float x = raw_a[bay][ph];
            d += into_bus_sign[bay] < 0 ? -x : x;
            float sq = x * x;
            st->sq_sum[bay][ph] += sq - st->sq[bay][ph][slot];
            st->sq[bay][ph][slot] = sq;
        }
        st->diff[ph][slot] = mimic_step(&st->mimic[ph], d);
    }
    st->n++;
}

/* Exact recomputation of one sliding sum per pass: bounds float drift to a
 * few passes' worth of rounding (every sum is re-anchored within 12 passes). */
static void reanchor(bus_diff_87b_state_t *st)
{
    uint32_t bay = st->anchor / IED_PHASES;
    uint32_t ph = st->anchor % IED_PHASES;
    float sum = 0.0f;
    for (uint32_t m = 0; m < BUS_DIFF_87B_WINDOW; m++) {
        sum += st->sq[bay][ph][m];
    }
    st->sq_sum[bay][ph] = sum;
    st->anchor = (st->anchor + 1u) % (IED_NUM_BAYS * IED_PHASES);
}

bool bus_diff_87b_step(const bus_diff_87b_settings_t *s, bus_diff_87b_state_t *st,
                       uint32_t invalid_mask)
{
    st->enabled = s->enabled != 0u;
    if (!st->enabled) {
        reset_logic(st);
        return false;
    }

    uint32_t zone_mask = 0u;
    for (uint32_t bay = 0; bay < IED_NUM_BAYS; bay++) {
        if (s->bay_in_zone[bay] != 0u) {
            zone_mask |= IED_BAY_MASK(bay);
        }
    }
    if ((invalid_mask & zone_mask) != 0u || s->base_a <= 0.0f) {
        /* Invalid is not zero: hold everything, including the CTS timers,
         * and re-arm so that no sample taken while invalid is ever decided on. */
        clear_quantities(st);
        st->blocked = true;
        st->valid_passes = 0u;
        return false;
    }
    if (st->valid_passes < BUS_DIFF_87B_ARM_PASSES || st->n < BUS_DIFF_87B_WINDOW) {
        st->valid_passes++;
        clear_quantities(st);
        st->blocked = true;
        reanchor(st);
        return false;
    }

    reanchor(st);
    float inv_base = 1.0f / s->base_a;
    const float k = CPLX_SQRT2 / (float)BUS_DIFF_87B_WINDOW;
    bool cts_cond = false;
    bool any_operate = false;
    uint8_t op_phase = 0u;
    for (uint32_t ph = 0; ph < IED_PHASES; ph++) {
        /* Full-cycle DFT of the differential samples, same reference as the
         * platform phasor estimator, then the mimic phase compensation. */
        float re = 0.0f;
        float im = 0.0f;
        for (uint32_t m = 0; m < BUS_DIFF_87B_WINDOW; m++) {
            uint32_t slot = (st->n - 1u - m) & (BUS_DIFF_87B_WINDOW - 1u);
            cplx_t ref = phasor_ref(st->sis[slot]);
            float x = st->diff[ph][slot];
            re += x * ref.re;
            im += x * ref.im;
        }
        cplx_t d = mimic_compensate(&st->mimic[ph], cplx(re * k, im * k));
        float id = cplx_abs(d) * inv_base;

        float restraint = 0.0f;
        for (uint32_t bay = 0; bay < IED_NUM_BAYS; bay++) {
            if (s->bay_in_zone[bay] == 0u) {
                continue;
            }
            float ms = st->sq_sum[bay][ph] / (float)BUS_DIFF_87B_WINDOW;
            restraint += sqrtf(ms > 0.0f ? ms : 0.0f);
        }
        float ir = restraint * inv_base;

        st->id_pu[ph] = id;
        st->ir_pu[ph] = ir;
        bool pickup = id > s->pickup_pu;
        bool operate = pickup && id > bus_diff_87b_threshold_pu(s, ir);
        st->pickup[ph] = pickup;
        st->operate[ph] = operate;
        if (operate && !any_operate) {
            op_phase = (uint8_t)ph;
        }
        any_operate = any_operate || operate;
        if (!pickup && ir >= BUS_DIFF_87B_CTS_MIN_LOAD_PU && id > s->cts_alarm_pct * 0.01f * ir) {
            cts_cond = true;
        }
    }

    /* CT supervision: symmetric pick-up / drop-out timer in protection passes. */
    if (cts_cond) {
        st->cts_clear_count = 0u;
        if (st->cts_count < s->cts_delay_passes) {
            st->cts_count++;
        }
        if (st->cts_count >= s->cts_delay_passes) {
            st->cts_alarm = true;
        }
    } else {
        st->cts_count = 0u;
        if (st->cts_alarm) {
            st->cts_clear_count++;
            if (st->cts_clear_count >= s->cts_delay_passes) {
                st->cts_alarm = false;
                st->cts_clear_count = 0u;
            }
        }
    }

    st->blocked = st->cts_alarm;
    if (st->blocked) {
        for (uint32_t ph = 0; ph < IED_PHASES; ph++) {
            st->operate[ph] = false;
        }
        any_operate = false;
    }
    st->op_phase = op_phase;
    st->operated = any_operate;
    return any_operate;
}
