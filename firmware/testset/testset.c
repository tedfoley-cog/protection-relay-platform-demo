#include "testset.h"

#include <string.h>

#define E_LN_V        7200.0f
#define SQRT3_OVER2   0.866025403784439f

/* Source behind the substation bus (20 MVA transformer, solidly grounded). */
static const cplx_t k_zs1 = {0.35f, 1.90f};
static const cplx_t k_zs0 = {0.30f, 1.70f};
/* 5 km of 336 ACSR. */
static const cplx_t k_zl1 = {1.53f, 3.14f};
static const cplx_t k_zl0 = {3.85f, 10.0f};

static const scenario_t k_scenarios[] = {
    {
        .name = "steady_load",
        .description = "Balanced 320 A load at 0.95 pf, 60 Hz, PPS locked",
        .duration_s = 1.0f, .freq_hz = 60.0f, .freq_end_hz = 60.0f,
        .load_a = 320.0f, .load_pf = 0.95f,
    },
    {
        .name = "fault_ag_zone1",
        .description = "A-G fault at 40% of the line, 0.5 ohm; cleared by 21 zone 1",
        .duration_s = 0.8f, .freq_hz = 60.0f, .freq_end_hz = 60.0f,
        .load_a = 320.0f, .load_pf = 0.95f,
        .fault = FAULT_AG, .fault_start_s = 0.30f, .fault_location_pct = 40.0f,
        .fault_r_ohm = 0.5f,
    },
    {
        .name = "fault_bc_zone2",
        .description = "B-C fault at 110% of the line; cleared by time-delayed 21 zone 2",
        .duration_s = 1.0f, .freq_hz = 60.0f, .freq_end_hz = 60.0f,
        .load_a = 320.0f, .load_pf = 0.95f,
        .fault = FAULT_BC, .fault_start_s = 0.30f, .fault_location_pct = 110.0f,
        .fault_r_ohm = 0.1f,
    },
    {
        .name = "fault_ag_high_resistance",
        .description = "A-G fault through 10 ohm at 50%; outside the mho, cleared by 51G",
        .duration_s = 2.2f, .freq_hz = 60.0f, .freq_end_hz = 60.0f,
        .load_a = 320.0f, .load_pf = 0.95f,
        .fault = FAULT_AG, .fault_start_s = 0.30f, .fault_location_pct = 50.0f,
        .fault_r_ohm = 10.0f,
    },
    {
        .name = "breaker_failure",
        .description = "A-G zone 1 fault with the feeder breaker stuck closed; 50BF trips the bus",
        .duration_s = 0.8f, .freq_hz = 60.0f, .freq_end_hz = 60.0f,
        .load_a = 320.0f, .load_pf = 0.95f,
        .fault = FAULT_AG, .fault_start_s = 0.30f, .fault_location_pct = 40.0f,
        .fault_r_ohm = 0.5f, .breaker_stuck = true,
    },
    {
        .name = "bus_fault_external",
        .description = "B-C fault at 5% of the bay-3 feeder, cleared by bay 3's own relay at 0.4 s; "
                       "external to the bus zone, bay currents sum to zero",
        .duration_s = 1.0f, .freq_hz = 60.0f, .freq_end_hz = 60.0f,
        .load_a = 320.0f, .load_pf = 0.95f,
        .fault = FAULT_BC, .fault_start_s = 0.30f, .fault_location_pct = 5.0f,
        .fault_r_ohm = 0.1f, .fault_zone = ZONE_BAY3_LINE, .external_clear_s = 0.40f,
    },
    {
        .name = "bus_fault_internal",
        .description = "A-G fault on the bus through 0.2 ohm; internal to the bus zone, "
                       "cleared only by the 87B bus differential through 86B",
        .duration_s = 1.0f, .freq_hz = 60.0f, .freq_end_hz = 60.0f,
        .load_a = 320.0f, .load_pf = 0.95f,
        .fault = FAULT_AG, .fault_start_s = 0.30f, .fault_location_pct = 0.0f,
        .fault_r_ohm = 0.2f, .fault_zone = ZONE_BUS,
    },
    {
        .name = "ct_lost_bay1_load",
        .description = "Balanced load with the bay-1 CT circuit lost: inputs invalid, "
                       "no element may operate on the missing data",
        .duration_s = 1.0f, .freq_hz = 60.0f, .freq_end_hz = 60.0f,
        .load_a = 320.0f, .load_pf = 0.95f, .ct_lost_bay = 1u,
    },
};

static const bay_config_t k_bays[IED_NUM_BAYS] = {
    {"Feeder 1", BAY_ROLE_FEEDER, 1200.0f, 5.0f, BAY_POLARITY_TOWARD_LINE, 1u},
    {"Transformer T1", BAY_ROLE_INFEED, 2000.0f, 5.0f, BAY_POLARITY_TOWARD_BUS, 1u},
    {"Feeder 3", BAY_ROLE_FEEDER, 600.0f, 5.0f, BAY_POLARITY_TOWARD_BUS, 1u},
    {"Feeder 4", BAY_ROLE_FEEDER, 800.0f, 5.0f, BAY_POLARITY_TOWARD_BUS, 1u},
};
#define BAY3_LOAD_A 180.0f
#define BAY4_LOAD_A 240.0f
#define INFEED_BAY  1u

const bay_config_t *testset_default_bays(void)
{
    return k_bays;
}

int testset_scenario_count(void)
{
    return (int)(sizeof(k_scenarios) / sizeof(k_scenarios[0]));
}

const scenario_t *testset_scenario(int index)
{
    if (index < 0 || index >= testset_scenario_count()) {
        return NULL;
    }
    return &k_scenarios[index];
}

const scenario_t *testset_find(const char *name)
{
    for (int k = 0; k < testset_scenario_count(); k++) {
        if (strcmp(k_scenarios[k].name, name) == 0) {
            return &k_scenarios[k];
        }
    }
    return NULL;
}

static cplx_t op_a(void) { return cplx(-0.5f, SQRT3_OVER2); }
static cplx_t op_a2(void) { return cplx(-0.5f, -SQRT3_OVER2); }

/* Phase quantities from sequence components with `ref` as the reference
 * phase; result written so that out[ref] = X0 + X1 + X2. */
static void seq_to_phase(cplx_t x0, cplx_t x1, cplx_t x2, int ref, cplx_t out[3])
{
    cplx_t a = op_a();
    cplx_t a2 = op_a2();
    cplx_t p0 = cplx_add(cplx_add(x0, x1), x2);
    cplx_t p1 = cplx_add(cplx_add(x0, cplx_mul(a2, x1)), cplx_mul(a, x2));
    cplx_t p2 = cplx_add(cplx_add(x0, cplx_mul(a, x1)), cplx_mul(a2, x2));
    /* Rotate so the reference phase sits at its own angle in the ABC frame. */
    cplx_t rot = ref == 0 ? cplx(1.0f, 0.0f) : (ref == 1 ? a2 : a);
    out[ref] = cplx_mul(p0, rot);
    out[(ref + 1) % 3] = cplx_mul(p1, rot);
    out[(ref + 2) % 3] = cplx_mul(p2, rot);
}

void testset_solve_fault(fault_type_t type, float location_pct, float rf_ohm, cplx_t v[3],
                         cplx_t i[3], float *x_over_r)
{
    float d = location_pct / 100.0f;
    cplx_t z1 = cplx_add(k_zs1, cplx_scale(k_zl1, d));
    cplx_t z0 = cplx_add(k_zs0, cplx_scale(k_zl0, d));
    cplx_t e = cplx(E_LN_V, 0.0f);
    cplx_t rf = cplx(rf_ohm, 0.0f);
    cplx_t zero = cplx(0.0f, 0.0f);
    cplx_t i0 = zero;
    cplx_t i1 = zero;
    cplx_t i2 = zero;
    int ref = 0;
    cplx_t loop = z1;

    switch (type) {
    case FAULT_AG:
    case FAULT_BG:
    case FAULT_CG: {
        ref = (int)(type - FAULT_AG);
        cplx_t zt = cplx_add(cplx_add(cplx_scale(z1, 2.0f), z0), cplx_scale(rf, 3.0f));
        i1 = cplx_div_safe(e, zt, 1e-6f);
        i2 = i1;
        i0 = i1;
        loop = cplx_add(cplx_scale(z1, 2.0f), z0);
        break;
    }
    case FAULT_AB:
    case FAULT_BC:
    case FAULT_CA: {
        /* Phase-phase fault: the unfaulted phase is the reference. */
        static const int unfaulted[3] = {2, 0, 1};
        ref = unfaulted[type - FAULT_AB];
        cplx_t zt = cplx_add(cplx_scale(z1, 2.0f), rf);
        i1 = cplx_div_safe(e, zt, 1e-6f);
        i2 = cplx_scale(i1, -1.0f);
        loop = cplx_scale(z1, 2.0f);
        break;
    }
    case FAULT_ABC:
        i1 = cplx_div_safe(e, cplx_add(z1, rf), 1e-6f);
        break;
    case FAULT_NONE:
    default:
        break;
    }

    /* Bus voltages: source EMF minus the drop across the source impedance. */
    cplx_t v1 = cplx_sub(e, cplx_mul(k_zs1, i1));
    cplx_t v2 = cplx_scale(cplx_mul(k_zs1, i2), -1.0f);
    cplx_t v0 = cplx_scale(cplx_mul(k_zs0, i0), -1.0f);

    /* The reference-phase EMF is at angle 0 in its own frame, so rotate the
     * source EMF consistently with seq_to_phase. */
    seq_to_phase(v0, v1, v2, ref, v);
    seq_to_phase(i0, i1, i2, ref, i);
    if (x_over_r != NULL) {
        *x_over_r = loop.re > 1e-6f ? loop.im / loop.re : 10.0f;
    }
}

static void load_flow(const scenario_t *sc, cplx_t v[3], cplx_t i[IED_NUM_BAYS][3],
                      cplx_t z_load[IED_NUM_BAYS])
{
    cplx_t a = op_a();
    cplx_t a2 = op_a2();
    float phi = acosf(sc->load_pf > 0.0f ? sc->load_pf : 1.0f);
    float load_a[IED_NUM_BAYS] = {sc->load_a, 0.0f, BAY3_LOAD_A, BAY4_LOAD_A};
    float total_a = load_a[0] + load_a[2] + load_a[3];
    cplx_t itot = cplx_polar(total_a, -phi);
    cplx_t va = cplx_sub(cplx(E_LN_V, 0.0f), cplx_mul(k_zs1, itot));
    v[0] = va;
    v[1] = cplx_mul(va, a2);
    v[2] = cplx_mul(va, a);
    for (uint32_t bay = 0; bay < IED_NUM_BAYS; bay++) {
        cplx_t ia = cplx_polar(load_a[bay], -phi);
        if (bay == INFEED_BAY) {
            ia = cplx_scale(itot, -1.0f);   /* out of the bus into the transformer = -infeed */
        }
        i[bay][0] = ia;
        i[bay][1] = cplx_mul(ia, a2);
        i[bay][2] = cplx_mul(ia, a);
        z_load[bay] = (bay != INFEED_BAY && load_a[bay] > 0.0f) ? cplx_div_safe(va, ia, 1e-6f)
                                                               : cplx(0.0f, 0.0f);
    }
}

/* Fault-state currents of every bay from the faulted-path solution: the
 * faulted bay carries i_path, the healthy feeders draw load from the
 * depressed bus voltage and the infeed supplies the sum. */
static void fault_flow(const testset_t *ts, fault_zone_t zone, const cplx_t v[3],
                       const cplx_t i_path[3], cplx_t i[IED_NUM_BAYS][3])
{
    uint32_t faulted = zone == ZONE_BAY3_LINE ? 2u : 0u;
    for (int ph = 0; ph < 3; ph++) {
        cplx_t sum = cplx(0.0f, 0.0f);
        for (uint32_t bay = 0; bay < IED_NUM_BAYS; bay++) {
            if (bay == INFEED_BAY) {
                continue;
            }
            cplx_t ib;
            if (zone != ZONE_BUS && bay == faulted) {
                ib = i_path[ph];
            } else {
                cplx_t z = ts->z_load[bay];
                ib = (z.re != 0.0f || z.im != 0.0f) ? cplx_div_safe(v[ph], z, 1e-6f)
                                                    : cplx(0.0f, 0.0f);
            }
            i[bay][ph] = ib;
            sum = cplx_add(sum, ib);
        }
        /* Bus fault: the source also feeds the fault; out-of-bus current of the
         * infeed bay is -(loads + fault). */
        if (zone == ZONE_BUS) {
            sum = cplx_add(sum, i_path[ph]);
        }
        i[INFEED_BAY][ph] = cplx_scale(sum, -1.0f);
    }
}

void testset_init(testset_t *ts, const scenario_t *sc, const float scale[IED_NUM_CHANNELS])
{
    memset(ts, 0, sizeof(*ts));
    ts->sc = sc;
    for (int ch = 0; ch < IED_NUM_CHANNELS; ch++) {
        ts->scale[ch] = scale[ch];
    }
    ts->freq_hz = sc->freq_hz;
    ts->lcg = 0x2545F491u;
    ts->breaker_timer = -1;
    ts->bus_timer = -1;
    for (uint32_t bay = 0; bay < IED_NUM_BAYS; bay++) {
        ts->polarity[bay] = (int8_t)k_bays[bay].polarity;
    }
    load_flow(sc, ts->v_pre, ts->i_pre, ts->z_load);
    if (sc->fault != FAULT_NONE) {
        float xr = 1.0f;
        cplx_t i_path[3];
        testset_solve_fault(sc->fault, sc->fault_location_pct, sc->fault_r_ohm, ts->v_flt,
                            i_path, &xr);
        fault_flow(ts, sc->fault_zone, ts->v_flt, i_path, ts->i_flt);
        ts->fault_tau_samples = (xr / (CPLX_TWO_PI * IED_F_NOMINAL_HZ)) * (float)IED_SAMPLE_RATE_HZ;
    }
}

void testset_set_preroll(testset_t *ts, uint32_t samples)
{
    ts->preroll_n = samples;
}

float testset_time_s(const testset_t *ts)
{
    return ((float)ts->n - (float)ts->preroll_n) / (float)IED_SAMPLE_RATE_HZ;
}

bool testset_done(const testset_t *ts)
{
    return testset_time_s(ts) >= ts->sc->duration_s;
}

static float inst(cplx_t phasor, float theta)
{
    /* Instantaneous value of an RMS phasor at system angle theta. */
    return CPLX_SQRT2 * (phasor.re * cosf(theta) - phasor.im * sinf(theta));
}

static int16_t adc(float value, float scale, uint32_t *lcg)
{
    *lcg = *lcg * 1664525u + 1013904223u;
    float dither = (float)((int32_t)(*lcg >> 30) - 1); /* -1, 0, +1, +2 LSB -> centred */
    float counts = value / scale + dither * 0.5f;
    if (counts > (float)IED_ADC_FULL_SCALE) {
        counts = (float)IED_ADC_FULL_SCALE;
    } else if (counts < -(float)IED_ADC_FULL_SCALE) {
        counts = -(float)IED_ADC_FULL_SCALE;
    }
    return (int16_t)lrintf(counts);
}

void testset_step(testset_t *ts, bool trip1, bool tripb, int16_t raw[IED_NUM_CHANNELS])
{
    const scenario_t *sc = ts->sc;
    float t = testset_time_s(ts);

    /* Frequency ramp. */
    if (sc->rocof_hz_s != 0.0f && t >= sc->ramp_start_s && t >= 0.0f) {
        float f = sc->freq_hz + sc->rocof_hz_s * (t - sc->ramp_start_s);
        if ((sc->rocof_hz_s < 0.0f && f < sc->freq_end_hz) ||
            (sc->rocof_hz_s > 0.0f && f > sc->freq_end_hz)) {
            f = sc->freq_end_hz;
        }
        ts->freq_hz = f;
    }
    float theta = (float)ts->phase_acc * (CPLX_TWO_PI / 4294967296.0f);

    /* Fault inception. */
    if (sc->fault != FAULT_NONE && !ts->fault_active && t >= sc->fault_start_s && t >= 0.0f &&
        !ts->breaker_open) {
        ts->fault_active = true;
        ts->fault_n = ts->n;
        for (uint32_t bay = 0; bay < IED_NUM_BAYS; bay++) {
            for (int ph = 0; ph < 3; ph++) {
                ts->dc_offset[bay][ph] =
                    inst(ts->i_flt[bay][ph], theta) - inst(ts->i_pre[bay][ph], theta);
            }
        }
    }
    if (sc->fault_zone == ZONE_BAY3_LINE && ts->fault_active && !ts->external_cleared) {
        float clear_s = sc->external_clear_s > 0.0f ? sc->external_clear_s : 0.4f;
        if (t >= sc->fault_start_s + clear_s) {
            ts->fault_active = false;
            ts->external_cleared = true;
        }
    }

    /* Breaker models. */
    if (trip1 && !ts->breaker_open && ts->breaker_timer < 0 && !sc->breaker_stuck) {
        /* Bay-1 breaker: clears a bay-1 line fault only. */
        ts->breaker_timer = TESTSET_BREAKER_SAMPLES;
    }
    if (tripb && !ts->bus_dead && ts->bus_timer < 0) {
        ts->bus_timer = TESTSET_BUS_SAMPLES;
    }
    if (ts->breaker_timer > 0 && --ts->breaker_timer == 0) {
        ts->breaker_open = true;
        if (sc->fault_zone == ZONE_BAY1_LINE) {
            ts->fault_active = false;
        }
    }
    if (ts->bus_timer > 0 && --ts->bus_timer == 0) {
        ts->bus_dead = true;
        ts->fault_active = false;
    }

    const cplx_t *v = ts->fault_active ? ts->v_flt : ts->v_pre;
    float decay = 0.0f;
    if (ts->fault_active && ts->fault_tau_samples > 0.0f) {
        decay = expf(-(float)(ts->n - ts->fault_n) / ts->fault_tau_samples);
    }

    for (int ph = 0; ph < 3; ph++) {
        float vv = ts->bus_dead ? 0.0f : inst(v[ph], theta);
        raw[IED_CH_VA + ph] = adc(vv, ts->scale[IED_CH_VA + ph], &ts->lcg);
    }

    float sum_bay1 = 0.0f;
    float bay1_dropped[3] = {0.0f, 0.0f, 0.0f};   /* bay-1 current removed by its open breaker */
    for (uint32_t bay = 0; bay < IED_NUM_BAYS; bay++) {
        const cplx_t *i = ts->fault_active ? ts->i_flt[bay] : ts->i_pre[bay];
        for (int ph = 0; ph < 3; ph++) {
            float ii = 0.0f;
            if (!ts->bus_dead) {
                ii = inst(i[ph], theta);
                if (ts->fault_active) {
                    /* Current cannot change instantaneously at inception. */
                    ii -= ts->dc_offset[bay][ph] * decay;
                }
                if (bay == 0u && ts->breaker_open) {
                    bay1_dropped[ph] = ii;
                    ii = 0.0f;
                } else if (k_bays[bay].role == BAY_ROLE_INFEED) {
                    /* Kirchhoff at the bus: the infeed no longer supplies bay 1. */
                    ii += bay1_dropped[ph];
                }
            }
            if (bay == 0u) {
                sum_bay1 += ii;
            }
            /* CT polarity: TOWARD_BUS reads current into the bus as positive. */
            float measured = ts->polarity[bay] == (int8_t)BAY_POLARITY_TOWARD_BUS ? -ii : ii;
            if (sc->ct_lost_bay == bay + 1u) {
                measured = 0.0f;
            }
            ied_channel_t ch = IED_CH_BAY_I(bay, ph);
            raw[ch] = adc(measured, ts->scale[ch], &ts->lcg);
        }
    }
    raw[IED_CH_IN] = adc(sc->ct_lost_bay == 1u ? 0.0f : sum_bay1, ts->scale[IED_CH_IN], &ts->lcg);

    ts->phase_acc += (uint32_t)((ts->freq_hz / (float)IED_SAMPLE_RATE_HZ) * 4294967296.0f);
    ts->n++;
}

uint32_t testset_invalid_mask(const testset_t *ts)
{
    uint8_t bay = ts->sc->ct_lost_bay;
    if (bay == 0u || bay > IED_NUM_BAYS) {
        return 0u;
    }
    uint32_t mask = IED_BAY_MASK(bay - 1u);
    if (bay == 1u) {
        mask |= IED_CH_BIT(IED_CH_IN);
    }
    return mask;
}

cplx_t testset_bay_current(const testset_t *ts, uint32_t bay, int phase)
{
    if (bay >= IED_NUM_BAYS || phase < 0 || phase > 2) {
        return cplx(0.0f, 0.0f);
    }
    if (ts->bus_dead || (bay == 0u && ts->breaker_open)) {
        return cplx(0.0f, 0.0f);
    }
    return ts->fault_active ? ts->i_flt[bay][phase] : ts->i_pre[bay][phase];
}
