#include "oc_element.h"

#include <math.h>
#include <stddef.h>

float toc_operate_time(const toc_settings_t *s, float multiple)
{
    if (s == NULL) {
        return -1.0f;
    }
    return toc_curve_operate_time(s->curve, s->tms, s->definite_s, multiple);
}

float toc_reset_time(const toc_settings_t *s, float multiple)
{
    if (s == NULL) {
        return 0.0f;
    }
    return toc_curve_reset_time(s->curve, s->tms, multiple);
}

void toc_init(toc_state_t *st)
{
    st->accum = 0.0f;
    st->picked_up = false;
    st->operated = false;
}

bool toc_step(const toc_settings_t *s, toc_state_t *st, float i_mag, float dt_s)
{
    if (s == NULL || st == NULL || s->pickup_a <= 0.0f) {
        return false;
    }
    float m = i_mag / s->pickup_a;

    if (m > 1.0f) {
        st->picked_up = true;
        float t_op = toc_operate_time(s, m);
        if (t_op > 0.0f) {
            st->accum += dt_s / t_op;
        }
        if (st->accum >= 1.0f) {
            st->accum = 1.0f;
            st->operated = true;
        }
        return st->operated;
    }

    if (m >= TOC_DROPOUT_RATIO && st->picked_up) {
        /* Between dropout and pickup: hold the accumulator. */
        return st->operated;
    }

    st->picked_up = false;
    st->operated = false;
    float t_reset = toc_reset_time(s, m);
    if (t_reset > 0.0f) {
        st->accum -= dt_s / t_reset;
        if (st->accum < 0.0f) {
            st->accum = 0.0f;
        }
    } else {
        st->accum = 0.0f;
    }
    return false;
}

void ioc_init(ioc_state_t *st)
{
    st->count = 0u;
    st->operated = false;
}

bool ioc_step(const ioc_settings_t *s, ioc_state_t *st, float i_mag)
{
    if (s == NULL || st == NULL || s->pickup_a <= 0.0f) {
        return false;
    }
    if (i_mag > s->pickup_a) {
        if (st->count < UINT8_MAX) {
            st->count++;
        }
        if (st->count >= s->security_passes) {
            st->operated = true;
        }
    } else if (i_mag < s->pickup_a * TOC_DROPOUT_RATIO) {
        st->count = 0u;
        st->operated = false;
    }
    return st->operated;
}
