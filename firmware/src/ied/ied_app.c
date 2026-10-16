#include "ied_app.h"

#include "c37118.h"
#include "cycle_stats.h"
#include "dnp3_outstation.h"
#include "phasor.h"
#include "pu_scale.h"
#include "sample_history.h"
#include "sample_ring.h"
#include "symcomp.h"

#include <math.h>
#include <string.h>

timebase_t g_timebase;

/* Analog and binary point maps for the DNP3 outstation. */
enum {
    AI_IA = 0, AI_IB, AI_IC, AI_IN, AI_V1_KV, AI_FREQ, AI_ROCOF, AI_TRIPS, AI_COUNT
};
enum {
    BI_TRIP1 = 0, BI_86B, BI_52B, BI_21Z1, BI_21Z2, BI_51P, BI_51G, BI_50BF,
    BI_81U, BI_81O, BI_81R, BI_PPS_LOCK, BI_87B, BI_COUNT
};

typedef struct {
    const ied_settings_t *s;
    ied_hooks_t hooks;

    sample_ring_t ring;
    sample_history_t hist;
    mimic_filter_t mimic[4];   /* IA IB IC IN */
    mimic_filter_t mimic_bus[IED_NUM_BAYS - 1u][IED_PHASES];   /* bays 2-4, identical filtering for 87B */

    ioc_state_t i50p;
    ioc_state_t i50g;
    toc_state_t t51p;
    toc_state_t t51g;
    distance_state_t dist;
    bf_state_t bf;
    freq_state_t freq;
    bus_diff_87b_state_t b87;
    trip_matrix_state_t matrix;
    volatile elem_mask_t freq_mask;
    volatile uint32_t fault_block_until;  /* tick; frequency elements blocked before it */

    pmu_estimator_t pmu;
    volatile bool pmu_pending;
    volatile uint32_t pmu_tick;
    volatile uint32_t pmu_soc;

    volatile bool breaker_open;
    bool breaker_open_seen;
    uint32_t last_soc;
    uint32_t last_sis;
    uint32_t last_tick;

    soe_ring_t soe;
    dnp3_outstation_t dnp3;
    uint8_t dnp3_buf[96];
    ied_status_t status;
} ied_t;

static ied_t s_ied;

#define FAULT_V2_RATIO       0.10f
#define FAULT_HOLDOFF_TICKS  (IED_SAMPLE_RATE_HZ / 2u)  /* 0.5 s after the fault clears */

static const ied_settings_t k_default_settings = {
    /* 16-bit ADC: 0.75 V primary per count (VT 7200:120); current scales come
     * from the bay table (bay 1: 1200/1200 = 1.0 A per count). */
    .scale = {0.75f, 0.75f, 0.75f},
    .bay = {
        {"Feeder 1", BAY_ROLE_FEEDER, 1200.0f, 5.0f, BAY_POLARITY_TOWARD_LINE, 1u},
        {"Transformer T1", BAY_ROLE_INFEED, 2000.0f, 5.0f, BAY_POLARITY_TOWARD_BUS, 1u},
        {"Feeder 3", BAY_ROLE_FEEDER, 600.0f, 5.0f, BAY_POLARITY_TOWARD_BUS, 1u},
        {"Feeder 4", BAY_ROLE_FEEDER, 800.0f, 5.0f, BAY_POLARITY_TOWARD_BUS, 1u},
    },
    .v_ln_nominal = 7200.0f,
    .x_over_r = 4.0f,
    .i50p = {.pickup_a = 6000.0f, .security_passes = 2u},
    .i50g = {.pickup_a = 6000.0f, .security_passes = 2u},
    .t51p = {.pickup_a = 600.0f, .tms = 0.5f, .curve = TOC_IEEE_VI, .definite_s = 0.0f},
    .t51g = {.pickup_a = 120.0f, .tms = 1.0f, .curve = TOC_IEEE_VI, .definite_s = 0.0f},
    .dist = {
        /* 5 km of 336 ACSR on a 12.47 kV feeder. */
        .z1_line = {1.53f, 3.14f},
        .k0 = {0.0f, 0.0f},  /* computed in ied_init from Z0 */
        .z1_reach_pct = 80.0f,
        .z2_reach_pct = 120.0f,
        .z2_delay_ticks = 576u,  /* 0.30 s */
        .min_loop_a = 200.0f,
        .min_residual_a = 150.0f,
        .z1_security = 2u,
        .memory_alpha = 0.05f,
    },
    .bf = {.pickup_a = 240.0f, .retrip_delay_s = 0.020f, .bf_delay_s = 0.150f},
    .freq = {
        .uf_pickup_hz = 59.3f, .uf_frames = 6u,
        .of_pickup_hz = 60.5f, .of_frames = 6u,
        .rocof_pickup_hz_s = 1.5f, .rocof_frames = 2u,
        .uv_inhibit_pu = 0.5f,
    },
    .b87 = {
        /* Prototype values under REQUIREMENTS.md assumptions A1 (base) and
         * A3 (slope order); customer defaults 60/25 are BLOCKED. */
        .enabled = 1u,
        .base_mva = 20.0f,
        .pickup_pu = 0.20f,
        .slope1_pct = 25.0f,
        .slope2_pct = 60.0f,
        .breakpoint_pu = 2.0f,
        .cts_alarm_pct = 5.0f,
        .cts_delay_s = 3.0f,
        .bay_in_zone = {1u, 1u, 1u, 1u},
    },
    .matrix = {
        .trip1_mask = ELEM_BIT(ELEM_50P) | ELEM_BIT(ELEM_50G) | ELEM_BIT(ELEM_51P) |
                      ELEM_BIT(ELEM_51G) | ELEM_BIT(ELEM_21Z1) | ELEM_BIT(ELEM_21Z2) |
                      ELEM_BIT(ELEM_81U) | ELEM_BIT(ELEM_81O) | ELEM_BIT(ELEM_81R) |
                      ELEM_BIT(ELEM_50BF_RETRIP) | ELEM_BIT(ELEM_87B),
        .tripb_mask = ELEM_BIT(ELEM_50BF_BUS) | ELEM_BIT(ELEM_87B),
    },
    .pmu_idcode = 4107u,
    .station = "BUS1 FDR1 RELAY",
};

static const cplx_t k_line_z0 = {3.85f, 10.0f};

static const char *const k_phasor_names[IED_PMU_NUM_PHASORS] = {
    "VA", "VB", "VC", "IA", "IB", "IC", "IN", "V1", "I1",
};
static const uint8_t k_phasor_is_current[IED_PMU_NUM_PHASORS] = {0, 0, 0, 1, 1, 1, 1, 0, 1};
static const char *const k_digital_names[16] = {
    "TRIP1", "86B", "21Z1", "51", "50BF", "81U", "81O", "81R",
    "52B", "PPS_LOCK", "87B", "", "", "", "", "",
};

static const ied_channel_t k_mimic_ch[4] = {IED_CH_IA, IED_CH_IB, IED_CH_IC, IED_CH_IN};
static const ied_channel_t k_pmu_ch[7] = {IED_CH_VA, IED_CH_VB, IED_CH_VC,
                                          IED_CH_IA, IED_CH_IB, IED_CH_IC, IED_CH_IN};
#define BAY1_INPUT_MASK (IED_BAY_MASK(0) | IED_CH_BIT(IED_CH_IN))

const ied_settings_t *ied_default_settings(void)
{
    static ied_settings_t finalized;
    static bool ready;
    if (!ready) {
        finalized = k_default_settings;
        ied_settings_finalize(&finalized);
        ready = true;
    }
    return &finalized;
}

void ied_settings_finalize(ied_settings_t *s)
{
    for (uint32_t bay = 0; bay < IED_NUM_BAYS; bay++) {
        float a_per_count = pu_a_per_count(s->bay[bay].ct_primary_a, IED_ADC_COUNTS_PER_CT_RATED);
        for (uint32_t ph = 0; ph < IED_PHASES; ph++) {
            s->scale[IED_CH_BAY_I(bay, ph)] = a_per_count;
        }
    }
    s->scale[IED_CH_IN] = s->scale[IED_CH_IA];
    float kv_ll = s->v_ln_nominal * 1.7320508f / 1000.0f;
    s->b87.base_a = pu_base_a_from_mva(s->b87.base_mva, kv_ll);
    s->b87.cts_delay_passes = (uint32_t)(s->b87.cts_delay_s * (float)IED_PROT_RATE_HZ + 0.5f);
}

static uint32_t disconnected_mask(const ied_settings_t *s)
{
    uint32_t m = 0u;
    for (uint32_t bay = 0; bay < IED_NUM_BAYS; bay++) {
        if (!s->bay[bay].ct_connected) {
            m |= IED_BAY_MASK(bay);
        }
    }
    return m;
}

static ied_settings_t s_active;

void ied_init(const ied_settings_t *s, uint32_t soc_at_boot, const ied_hooks_t *hooks)
{
    memset(&s_ied, 0, sizeof(s_ied));
    s_active = s != NULL ? *s : k_default_settings;
    ied_settings_finalize(&s_active);
    if (s_active.dist.k0.re == 0.0f && s_active.dist.k0.im == 0.0f) {
        cplx_t num = cplx_sub(k_line_z0, s_active.dist.z1_line);
        s_active.dist.k0 = cplx_div_safe(num, cplx_scale(s_active.dist.z1_line, 3.0f), 1e-6f);
    }
    s_ied.s = &s_active;
    if (hooks != NULL) {
        s_ied.hooks = *hooks;
    }

    phasor_tables_init();
    cycle_stats_init();
    timebase_init(&g_timebase, soc_at_boot);
    sample_ring_init(&s_ied.ring);
    sample_history_init(&s_ied.hist);
    for (int k = 0; k < 4; k++) {
        mimic_init(&s_ied.mimic[k], s_ied.s->x_over_r);
    }
    for (uint32_t bay = 1; bay < IED_NUM_BAYS; bay++) {
        for (uint32_t ph = 0; ph < IED_PHASES; ph++) {
            mimic_init(&s_ied.mimic_bus[bay - 1u][ph], s_ied.s->x_over_r);
        }
    }
    ioc_init(&s_ied.i50p);
    ioc_init(&s_ied.i50g);
    toc_init(&s_ied.t51p);
    toc_init(&s_ied.t51g);
    distance_init(&s_ied.dist);
    bf_init(&s_ied.bf);
    freq_init(&s_ied.freq);
    bus_diff_87b_init(&s_ied.b87);
    trip_matrix_init(&s_ied.matrix);
    pmu_estimator_init(&s_ied.pmu);
    soe_init(&s_ied.soe);

    dnp3_init(&s_ied.dnp3, BI_COUNT, AI_COUNT);
    for (int k = AI_IA; k <= AI_IN; k++) {
        s_ied.dnp3.analog[k].scale = 1.0;
    }
    s_ied.dnp3.analog[AI_V1_KV].scale = 0.001;
    s_ied.dnp3.analog[AI_FREQ].scale = 0.001;
    s_ied.dnp3.analog[AI_ROCOF].scale = 0.001;
    s_ied.dnp3.analog[AI_TRIPS].scale = 1.0;
    s_ied.status.freq_hz = IED_F_NOMINAL_HZ;
}

/* ---------------------------------------------------------------- acq */

uint32_t ied_acq_isr(const int16_t raw[IED_NUM_CHANNELS])
{
    return ied_acq_isr_q(raw, 0u);
}

uint32_t ied_acq_isr_q(const int16_t raw[IED_NUM_CHANNELS], uint32_t invalid_mask)
{
    uint32_t t0 = rt_cycles();
    uint32_t reload = timebase_on_sample(&g_timebase);

    sample_frame_t frame;
    frame.invalid_mask = invalid_mask | disconnected_mask(s_ied.s);
    frame.tick = g_timebase.tick;
    frame.uptime_s = g_timebase.uptime_s;
    frame.soc = g_timebase.soc;
    frame.sample_in_second = (uint16_t)g_timebase.sample_in_second;
    for (uint32_t ch = 0; ch < (uint32_t)IED_NUM_CHANNELS; ch++) {
        frame.raw[ch] = raw[ch];
    }
    (void)sample_ring_push(&s_ied.ring, &frame);

    cycle_stats_record(RT_TASK_ACQ, t0);
    return reload;
}

void ied_pps_isr(uint32_t counts_since_last_pps)
{
    bool was_locked = g_timebase.pps_locked;
    timebase_on_pps(&g_timebase, counts_since_last_pps);
    if (g_timebase.pps_locked && !was_locked) {
        soe_event_t ev = {g_timebase.soc, 0u, g_timebase.tick, SOE_PPS_LOCK, 0u, 0.0f};
        soe_record(&s_ied.soe, &ev);
    }
}

/* --------------------------------------------------------- protection */

static void record(uint16_t code, uint16_t detail, float value)
{
    soe_event_t ev;
    ev.soc = s_ied.last_soc;
    ev.usec = timebase_fracsec(s_ied.last_sis, 1000000u);
    ev.tick = s_ied.last_tick;
    ev.code = code;
    ev.detail = detail;
    ev.value = value;
    soe_record(&s_ied.soe, &ev);
}

static void ingest(const sample_frame_t *f)
{
    float value[IED_NUM_CHANNELS];
    for (uint32_t ch = 0; ch < (uint32_t)IED_NUM_CHANNELS; ch++) {
        value[ch] = (float)f->raw[ch] * s_ied.s->scale[ch];
    }
    for (uint32_t k = 0; k < 4u; k++) {
        value[k_mimic_ch[k]] = mimic_step(&s_ied.mimic[k], value[k_mimic_ch[k]]);
    }
    /* Bays 2-4 get the same mimic as bay 1 so that a through current seen by
     * two bays cancels exactly in the 87B differential during the DC-offset
     * transient (linear, identical filters; REQ-87B-034). */
    for (uint32_t bay = 1; bay < IED_NUM_BAYS; bay++) {
        for (uint32_t ph = 0; ph < IED_PHASES; ph++) {
            ied_channel_t ch = IED_CH_BAY_I(bay, ph);
            value[ch] = mimic_step(&s_ied.mimic_bus[bay - 1u][ph], value[ch]);
        }
    }
    sample_history_push(&s_ied.hist, f->tick, f->sample_in_second, value);
    if (f->invalid_mask != s_ied.status.invalid_mask) {
        uint32_t m = f->invalid_mask;
        uint16_t first = 0u;
        while (m != 0u && (m & 1u) == 0u) {
            m >>= 1;
            first++;
        }
        s_ied.status.invalid_mask = f->invalid_mask;
        record(f->invalid_mask != 0u ? SOE_INPUT_INVALID : SOE_INPUT_VALID, first, 0.0f);
    }
    s_ied.last_soc = f->soc;
    s_ied.last_sis = f->sample_in_second;
    s_ied.last_tick = f->tick;
}

static void protection_pass(const sample_frame_t *f)
{
    const ied_settings_t *s = s_ied.s;
    ied_status_t *st = &s_ied.status;
    uint32_t tick = f->tick;

    cplx_t v[3];
    cplx_t i[3];
    for (int ph = 0; ph < 3; ph++) {
        v[ph] = phasor_full_cycle(&s_ied.hist, (ied_channel_t)(IED_CH_VA + ph), tick);
        i[ph] = phasor_full_cycle(&s_ied.hist, (ied_channel_t)(IED_CH_IA + ph), tick);
    }
    for (int ph = 0; ph < 3; ph++) {
        i[ph] = mimic_compensate(&s_ied.mimic[ph], i[ph]);
    }
    float in_a = cplx_abs(phasor_full_cycle(&s_ied.hist, IED_CH_IN, tick));
    float iph[3] = {cplx_abs(i[0]), cplx_abs(i[1]), cplx_abs(i[2])};
    float imax = fmaxf(iph[0], fmaxf(iph[1], iph[2]));

    /* 87B: every bay's phasors from this same frame (REQ-87B-003), current
     * into the bus positive, all mimic-compensated identically (Q1 closed
     * for this element, see REQ-87B-034). The element blocks itself on any
     * invalid in-zone channel, so it runs before the bay-1 hold below. */
    cplx_t i_bus[IED_NUM_BAYS][IED_PHASES];
    ied_channel_t bus_ch[(IED_NUM_BAYS - 1u) * IED_PHASES];
    cplx_t bus_ph[(IED_NUM_BAYS - 1u) * IED_PHASES];
    uint32_t n_bus = 0u;
    for (uint32_t bay = 1; bay < IED_NUM_BAYS; bay++) {
        if (s->b87.bay_in_zone[bay] != 0u) {
            for (uint32_t ph = 0; ph < IED_PHASES; ph++) {
                bus_ch[n_bus++] = IED_CH_BAY_I(bay, ph);
            }
        }
    }
    phasor_full_cycle_multi(&s_ied.hist, bus_ch, n_bus, tick, bus_ph);
    n_bus = 0u;
    for (uint32_t bay = 0; bay < IED_NUM_BAYS; bay++) {
        bool in_zone = s->b87.bay_in_zone[bay] != 0u;
        bool toward_bus = s->bay[bay].polarity == BAY_POLARITY_TOWARD_BUS;
        for (uint32_t ph = 0; ph < IED_PHASES; ph++) {
            cplx_t m = cplx(0.0f, 0.0f);
            if (in_zone) {
                m = bay == 0u ? i[ph] : mimic_compensate(&s_ied.mimic_bus[bay - 1u][ph], bus_ph[n_bus++]);
            }
            i_bus[bay][ph] = toward_bus ? m : cplx_scale(m, -1.0f);
        }
    }
    elem_mask_t op = 0u;
    if (bus_diff_87b_step(&s->b87, &s_ied.b87, i_bus, f->invalid_mask)) {
        op |= ELEM_BIT(ELEM_87B);
    }
    if (s_ied.b87.blocked != st->b87.blocked && s_ied.b87.enabled) {
        record(s_ied.b87.blocked ? SOE_ELEMENT_BLOCKED : SOE_ELEMENT_UNBLOCKED, ELEM_87B,
               s_ied.b87.cts_alarm ? 1.0f : 0.0f);
    }
    if (s_ied.b87.cts_alarm != st->b87.cts_alarm) {
        record(s_ied.b87.cts_alarm ? SOE_CTS_ALARM : SOE_CTS_ALARM_RESET, ELEM_87B,
               s_ied.b87.id_pu[0]);
    }
    st->b87 = s_ied.b87;

    if ((f->invalid_mask & BAY1_INPUT_MASK) != 0u) {
        /* Bay-1 CT data missing or invalid: hold every element that reads it.
         * Frequency elements run on the VTs and are unaffected; 87B has
         * already blocked itself unless bay 1 is out of its zone. */
        op |= s_ied.freq_mask;
        bool tripb_was = s_ied.matrix.tripb;
        trip_matrix_step(&s->matrix, &s_ied.matrix, op, s_ied.breaker_open, false);
        if ((s_ied.matrix.rising & ELEM_BIT(ELEM_87B)) != 0u) {
            record(SOE_ELEMENT_OPERATE, ELEM_87B, s_ied.b87.id_pu[s_ied.b87.op_phase]);
        }
        if (s_ied.matrix.tripb && !tripb_was) {
            record(SOE_TRIPB_ON, ELEM_87B, s_ied.b87.id_pu[s_ied.b87.op_phase]);
        }
        st->tripb = s_ied.matrix.tripb;
        st->operated = s_ied.matrix.operated;
        st->target = s_ied.matrix.target;
        st->invalid_mask = f->invalid_mask;
        st->blocked_passes++;
        st->protection_passes++;
        return;
    }
    if (ioc_step(&s->i50p, &s_ied.i50p, imax)) {
        op |= ELEM_BIT(ELEM_50P);
    }
    if (ioc_step(&s->i50g, &s_ied.i50g, in_a)) {
        op |= ELEM_BIT(ELEM_50G);
    }
    if (toc_step(&s->t51p, &s_ied.t51p, imax, IED_PROT_PERIOD_S)) {
        op |= ELEM_BIT(ELEM_51P);
    }
    if (toc_step(&s->t51g, &s_ied.t51g, in_a, IED_PROT_PERIOD_S)) {
        op |= ELEM_BIT(ELEM_51G);
    }

    distance_step(&s->dist, &s_ied.dist, v, i, s->v_ln_nominal, tick);
    if (s_ied.dist.z1_operated) {
        op |= ELEM_BIT(ELEM_21Z1);
    }
    if (s_ied.dist.z2_operated) {
        op |= ELEM_BIT(ELEM_21Z2);
    }
    op |= s_ied.freq_mask;

    float now_s = timebase_seconds(f->uptime_s, f->sample_in_second);
    bf_step(&s->bf, &s_ied.bf, s_ied.matrix.trip1, iph, now_s);
    if (s_ied.bf.retrip) {
        op |= ELEM_BIT(ELEM_50BF_RETRIP);
    }
    if (s_ied.bf.bus_trip) {
        op |= ELEM_BIT(ELEM_50BF_BUS);
    }

    symcomp_t vs = symcomp_from_abc(v[0], v[1], v[2]);
    bool fault_detected = imax > s->t51p.pickup_a || in_a > s->t51g.pickup_a ||
                          s_ied.dist.z2_loops != 0u || s_ied.matrix.trip1 ||
                          cplx_abs(vs.neg) > FAULT_V2_RATIO * cplx_abs(vs.pos);
    if (fault_detected) {
        s_ied.fault_block_until = tick + FAULT_HOLDOFF_TICKS;
    }

    bool breaker_open = s_ied.breaker_open;
    bool current_present = iph[0] > s->bf.pickup_a || iph[1] > s->bf.pickup_a ||
                           iph[2] > s->bf.pickup_a;
    bool trip1_before = s_ied.matrix.trip1;
    bool tripb_before = s_ied.matrix.tripb;
    trip_matrix_step(&s->matrix, &s_ied.matrix, op, breaker_open, current_present);

    elem_mask_t rising = s_ied.matrix.rising;
    for (int e = 0; e < ELEM_COUNT; e++) {
        if ((rising & ELEM_BIT(e)) != 0u) {
            float value = imax;
            if (e == ELEM_50G || e == ELEM_51G) {
                value = in_a;
            } else if (e == ELEM_50BF_RETRIP || e == ELEM_50BF_BUS) {
                value = s_ied.bf.elapsed_s;
            } else if (e == ELEM_87B) {
                value = s_ied.b87.id_pu[s_ied.b87.op_phase];
            } else if (e >= ELEM_81U && e <= ELEM_81R) {
                value = e == ELEM_81R ? st->rocof_hz_s : st->freq_hz;
            }
            uint16_t detail = (uint16_t)e;
            if (e == ELEM_21Z1 || e == ELEM_21Z2) {
                uint8_t loops = e == ELEM_21Z1 ? s_ied.dist.z1_loops : s_ied.dist.z2_loops;
                detail = (uint16_t)(e | ((uint16_t)loops << 8));
            }
            record(SOE_ELEMENT_OPERATE, detail, value);
        }
    }
    if (s_ied.matrix.trip1 && !trip1_before) {
        record(SOE_TRIP1_ON, (uint16_t)s_ied.matrix.target, imax);
    } else if (!s_ied.matrix.trip1 && trip1_before) {
        record(SOE_TRIP1_OFF, 0u, imax);
    }
    if (s_ied.matrix.tripb && !tripb_before) {
        if ((op & ELEM_BIT(ELEM_87B)) != 0u) {
            record(SOE_TRIPB_ON, ELEM_87B, s_ied.b87.id_pu[s_ied.b87.op_phase]);
        } else {
            record(SOE_TRIPB_ON, ELEM_50BF_BUS, s_ied.bf.elapsed_s);
        }
    }
    if (breaker_open != s_ied.breaker_open_seen) {
        record(breaker_open ? SOE_BREAKER_OPEN : SOE_BREAKER_CLOSED, 0u, imax);
        s_ied.breaker_open_seen = breaker_open;
    }

    for (int ph = 0; ph < 3; ph++) {
        st->v[ph] = v[ph];
        st->i[ph] = i[ph];
    }
    st->in_a = in_a;
    st->operated = s_ied.matrix.operated;
    st->target = s_ied.matrix.target;
    st->trip1 = s_ied.matrix.trip1;
    st->tripb = s_ied.matrix.tripb;
    st->breaker_open = breaker_open;
    st->protection_passes++;
}

bool ied_protection_task(void)
{
    uint32_t t0 = rt_cycles();
    bool pmu_due = false;
    bool ran = false;
    sample_frame_t f;

    while (sample_ring_pop(&s_ied.ring, &f)) {
        ingest(&f);
        if (s_ied.hist.count < IED_SAMPLES_PER_CYCLE) {
            continue;
        }
        if ((f.sample_in_second % IED_PROT_DECIMATION) == IED_PROT_DECIMATION - 1u) {
            protection_pass(&f);
            ran = true;
        }
        if (s_ied.hist.count >= PMU_WINDOW_TAPS && pmu_frame_due(f.sample_in_second)) {
            if (s_ied.pmu_pending) {
                s_ied.status.pmu_overruns++;
                record(SOE_TASK_OVERRUN, RT_TASK_PMU, 0.0f);
            }
            s_ied.pmu_tick = f.tick;
            s_ied.pmu_soc = f.soc;
            s_ied.pmu_pending = true;
            pmu_due = true;
        }
    }
    s_ied.status.ring_overruns = s_ied.ring.overruns;
    if (ran) {
        cycle_stats_record(RT_TASK_PROT, t0);
    }
    return pmu_due;
}

/* ---------------------------------------------------------------- pmu */

static uint16_t digital_word(void)
{
    const trip_matrix_state_t *m = &s_ied.matrix;
    uint16_t d = 0u;
    d |= m->trip1 ? 0x0001u : 0u;
    d |= m->tripb ? 0x0002u : 0u;
    d |= (m->operated & ELEM_BIT(ELEM_21Z1)) != 0u ? 0x0004u : 0u;
    d |= (m->operated & (ELEM_BIT(ELEM_51P) | ELEM_BIT(ELEM_51G))) != 0u ? 0x0008u : 0u;
    d |= (m->operated & ELEM_BIT(ELEM_50BF_BUS)) != 0u ? 0x0010u : 0u;
    d |= s_ied.freq.uf_operated ? 0x0020u : 0u;
    d |= s_ied.freq.of_operated ? 0x0040u : 0u;
    d |= s_ied.freq.rocof_operated ? 0x0080u : 0u;
    d |= s_ied.breaker_open ? 0x0100u : 0u;
    d |= g_timebase.pps_locked ? 0x0200u : 0u;
    d |= s_ied.b87.operated ? 0x0400u : 0u;
    return d;
}

void ied_pmu_task(void)
{
    if (!s_ied.pmu_pending) {
        return;
    }
    uint32_t t0 = rt_cycles();
    uint32_t tick = s_ied.pmu_tick;
    uint32_t soc = s_ied.pmu_soc;

    pmu_measurement_t *m = pmu_estimate(&s_ied.pmu, &s_ied.hist, tick);
    for (uint32_t k = 0; k < 4u; k++) {
        m->phasor[k_mimic_ch[k]] = mimic_compensate(&s_ied.mimic[k], m->phasor[k_mimic_ch[k]]);
    }
    m->i1 = mimic_compensate(&s_ied.mimic[0], m->i1);
    float v1_pu = cplx_abs(m->v1) / s_ied.s->v_ln_nominal;
    bool fault_block = (int32_t)(s_ied.fault_block_until - tick) > 0;

    if (m->valid) {
        freq_step(&s_ied.s->freq, &s_ied.freq, m->freq_hz, m->rocof_hz_s, v1_pu, fault_block);
    }
    elem_mask_t fm = 0u;
    fm |= s_ied.freq.uf_operated ? ELEM_BIT(ELEM_81U) : 0u;
    fm |= s_ied.freq.of_operated ? ELEM_BIT(ELEM_81O) : 0u;
    fm |= s_ied.freq.rocof_operated ? ELEM_BIT(ELEM_81R) : 0u;
    s_ied.freq_mask = fm;

    c37118_data_t d;
    d.idcode = s_ied.s->pmu_idcode;
    d.soc = soc;
    d.fracsec = ((uint32_t)timebase_time_quality(&g_timebase) << 24) |
                timebase_fracsec(m->sample_in_second, C37118_TIME_BASE);
    d.stat = timebase_stat_bits(&g_timebase);
    if (!g_timebase.pps_locked) {
        d.stat |= C37118_STAT_NOT_SYNC;
    }
    if (!m->valid) {
        d.stat |= C37118_STAT_DATA_INVALID;
    }
    if (fm != 0u) {
        d.stat |= C37118_STAT_TRIGGER |
                  (s_ied.freq.rocof_operated ? C37118_TRIG_DFDT : C37118_TRIG_FREQ);
    } else if (s_ied.matrix.trip1 || s_ied.matrix.tripb) {
        d.stat |= C37118_STAT_TRIGGER | C37118_TRIG_DIGITAL;
    }
    d.num_phasors = IED_PMU_NUM_PHASORS;
    for (uint32_t k = 0; k < 7u; k++) {
        d.phasor[k] = m->phasor[k_pmu_ch[k]];
    }
    d.phasor[7] = m->v1;
    d.phasor[8] = m->i1;
    d.freq_hz = m->freq_hz;
    d.rocof_hz_s = m->rocof_hz_s;
    d.digital = digital_word();

    uint8_t frame[C37118_MAX_FRAME];
    size_t len = c37118_encode_data(&d, frame, sizeof(frame));

    s_ied.status.freq_hz = m->freq_hz;
    s_ied.status.rocof_hz_s = m->rocof_hz_s;
    s_ied.status.pps_locked = g_timebase.pps_locked;
    s_ied.status.pmu_frames++;
    s_ied.pmu_pending = false;

    cycle_stats_record(RT_TASK_PMU, t0);

    if (s_ied.hooks.pmu_frame != NULL && len > 0u) {
        s_ied.hooks.pmu_frame(frame, len, m, d.soc, d.fracsec, s_ied.hooks.ctx);
    }
}

/* -------------------------------------------------------------- comms */

void ied_comms_task(void)
{
    uint32_t t0 = rt_cycles();
    const ied_status_t *st = &s_ied.status;
    const trip_matrix_state_t *m = &s_ied.matrix;
    bool online = true;

    dnp3_set_analog(&s_ied.dnp3, AI_IA, cplx_abs(st->i[0]), online);
    dnp3_set_analog(&s_ied.dnp3, AI_IB, cplx_abs(st->i[1]), online);
    dnp3_set_analog(&s_ied.dnp3, AI_IC, cplx_abs(st->i[2]), online);
    dnp3_set_analog(&s_ied.dnp3, AI_IN, st->in_a, online);
    symcomp_t vs = symcomp_from_abc(st->v[0], st->v[1], st->v[2]);
    dnp3_set_analog(&s_ied.dnp3, AI_V1_KV, cplx_abs(vs.pos) / 1000.0f, online);
    dnp3_set_analog(&s_ied.dnp3, AI_FREQ, st->freq_hz, online);
    dnp3_set_analog(&s_ied.dnp3, AI_ROCOF, st->rocof_hz_s, online);
    dnp3_set_analog(&s_ied.dnp3, AI_TRIPS, (double)m->trip1_count, online);

    dnp3_set_binary(&s_ied.dnp3, BI_TRIP1, m->trip1, online);
    dnp3_set_binary(&s_ied.dnp3, BI_86B, m->tripb, online);
    dnp3_set_binary(&s_ied.dnp3, BI_52B, st->breaker_open, online);
    dnp3_set_binary(&s_ied.dnp3, BI_21Z1, (m->operated & ELEM_BIT(ELEM_21Z1)) != 0u, online);
    dnp3_set_binary(&s_ied.dnp3, BI_21Z2, (m->operated & ELEM_BIT(ELEM_21Z2)) != 0u, online);
    dnp3_set_binary(&s_ied.dnp3, BI_51P, (m->operated & ELEM_BIT(ELEM_51P)) != 0u, online);
    dnp3_set_binary(&s_ied.dnp3, BI_51G, (m->operated & ELEM_BIT(ELEM_51G)) != 0u, online);
    dnp3_set_binary(&s_ied.dnp3, BI_50BF, m->tripb, online);
    dnp3_set_binary(&s_ied.dnp3, BI_81U, s_ied.freq.uf_operated, online);
    dnp3_set_binary(&s_ied.dnp3, BI_81O, s_ied.freq.of_operated, online);
    dnp3_set_binary(&s_ied.dnp3, BI_81R, s_ied.freq.rocof_operated, online);
    dnp3_set_binary(&s_ied.dnp3, BI_PPS_LOCK, g_timebase.pps_locked, online);
    dnp3_set_binary(&s_ied.dnp3, BI_87B, st->b87.operated, online);
    (void)dnp3_encode_class0(&s_ied.dnp3, s_ied.dnp3_buf, sizeof(s_ied.dnp3_buf));

    soe_event_t ev;
    uint32_t drained = 0u;
    while (drained < 8u && soe_pop(&s_ied.soe, &ev)) {
        if (s_ied.hooks.event != NULL) {
            s_ied.hooks.event(&ev, s_ied.hooks.ctx);
        }
        drained++;
    }
    cycle_stats_record(RT_TASK_COMMS, t0);
}

/* ----------------------------------------------------------- external */

void ied_set_breaker_open(bool open)
{
    s_ied.breaker_open = open;
}

void ied_reset_lockout(void)
{
    trip_matrix_reset_lockout(&s_ied.matrix);
    bf_reset(&s_ied.bf);
}

const ied_status_t *ied_status(void)
{
    return &s_ied.status;
}

size_t ied_pmu_config_frame(uint8_t *buf, size_t len)
{
    c37118_cfg_t cfg = {
        .idcode = s_ied.s->pmu_idcode,
        .station = s_ied.s->station,
        .num_phasors = IED_PMU_NUM_PHASORS,
        .phasor_names = k_phasor_names,
        .phasor_is_current = k_phasor_is_current,
        .phasor_scale = NULL,
        .digital_names = k_digital_names,
        .cfgcnt = 1u,
        .data_rate = (int16_t)IED_PMU_RATE_HZ,
        .soc = g_timebase.soc,
    };
    return c37118_encode_cfg2(&cfg, buf, len);
}

size_t ied_dnp3_class0(uint8_t *buf, size_t len)
{
    return dnp3_encode_class0(&s_ied.dnp3, buf, len);
}
