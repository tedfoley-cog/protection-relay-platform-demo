#include "bus_diff_87b.h"

void bus_diff_87b_init(bus_diff_87b_state_t *st)
{
    for (uint32_t ph = 0; ph < IED_PHASES; ph++) {
        st->id_pu[ph] = 0.0f;
        st->ir_pu[ph] = 0.0f;
        st->pickup[ph] = false;
        st->operate[ph] = false;
    }
    st->operated = false;
    st->blocked = false;
    st->cts_alarm = false;
    st->enabled = false;
    st->cts_count = 0u;
    st->cts_clear_count = 0u;
    st->op_phase = 0u;
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

bool bus_diff_87b_step(const bus_diff_87b_settings_t *s, bus_diff_87b_state_t *st,
                       const cplx_t i_into_bus[IED_NUM_BAYS][IED_PHASES], uint32_t invalid_mask)
{
    st->enabled = s->enabled != 0u;
    if (!st->enabled) {
        bus_diff_87b_init(st);
        return false;
    }

    uint32_t zone_mask = 0u;
    for (uint32_t bay = 0; bay < IED_NUM_BAYS; bay++) {
        if (s->bay_in_zone[bay] != 0u) {
            zone_mask |= IED_BAY_MASK(bay);
        }
    }
    if ((invalid_mask & zone_mask) != 0u || s->base_a <= 0.0f) {
        /* Invalid is not zero: hold everything, including the CTS timers. */
        clear_quantities(st);
        st->blocked = true;
        return false;
    }

    float inv_base = 1.0f / s->base_a;
    bool cts_cond = false;
    bool any_pickup = false;
    bool any_operate = false;
    uint8_t op_phase = 0u;
    for (uint32_t ph = 0; ph < IED_PHASES; ph++) {
        cplx_t sum = cplx(0.0f, 0.0f);
        float restraint = 0.0f;
        for (uint32_t bay = 0; bay < IED_NUM_BAYS; bay++) {
            if (s->bay_in_zone[bay] == 0u) {
                continue;
            }
            sum = cplx_add(sum, i_into_bus[bay][ph]);
            restraint += cplx_abs(i_into_bus[bay][ph]);
        }
        float id = cplx_abs(sum) * inv_base;
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
        any_pickup = any_pickup || pickup;
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
    (void)any_pickup;
    st->op_phase = op_phase;
    st->operated = any_operate;
    return any_operate;
}
