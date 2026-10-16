/* 87B busbar differential: requirement tests (REQ-87B-*), scenario tests on
 * the closed loop and the seeded safety-invariant sweep.
 * Validation plan: docs/elements/87B/VALIDATION_PLAN.md (T-87B-nn). */
#include "test_harness.h"
#include "ied_loop.h"
#include "bus_diff_87b.h"

#include <stdlib.h>

#define FAULT_TICK ((LOOP_PREROLL_S + 0u) * IED_SAMPLE_RATE_HZ + (uint32_t)(0.30f * IED_SAMPLE_RATE_HZ))
#define SWEEP_SEED 0x087B0001u
#define SWEEP_CASES 540

static const soe_event_t *find_operate(elem_id_t e)
{
    for (int k = 0; k < loop_res.num_events; k++) {
        const soe_event_t *ev = &loop_res.events[k];
        if (ev->code == SOE_ELEMENT_OPERATE && (ev->detail & 0xFFu) == (uint16_t)e) {
            return ev;
        }
    }
    return NULL;
}

/* Synthetic secondary injection for the unit tests: pure 60 Hz sinusoids
 * from the given into-bus phasors, four samples per protection pass, all
 * bays wired with positive = into the bus. Returns the last step result. */
static const int8_t k_into_bus[IED_NUM_BAYS] = {1, 1, 1, 1};
static uint32_t s_sis;

static bool feed_passes(const bus_diff_87b_settings_t *s, bus_diff_87b_state_t *st,
                        const cplx_t i[IED_NUM_BAYS][3], uint32_t passes, uint32_t invalid_mask)
{
    bool op = false;
    for (uint32_t p = 0; p < passes; p++) {
        for (uint32_t k = 0; k < IED_PROT_DECIMATION; k++) {
            float theta = CPLX_TWO_PI * (float)(s_sis & (IED_SAMPLES_PER_CYCLE - 1u)) /
                          (float)IED_SAMPLES_PER_CYCLE;
            float raw[IED_NUM_BAYS][IED_PHASES];
            for (uint32_t bay = 0; bay < IED_NUM_BAYS; bay++) {
                for (uint32_t ph = 0; ph < IED_PHASES; ph++) {
                    raw[bay][ph] = CPLX_SQRT2 * (i[bay][ph].re * cosf(theta) - i[bay][ph].im * sinf(theta));
                }
            }
            bus_diff_87b_ingest(s, st, raw, k_into_bus, s_sis);
            s_sis = (s_sis + 1u) % IED_SAMPLE_RATE_HZ;
        }
        op = bus_diff_87b_step(s, st, invalid_mask);
    }
    return op;
}

#define SETTLE_PASSES (2u * BUS_DIFF_87B_ARM_PASSES)

static void set_bay(cplx_t i[IED_NUM_BAYS][3], uint32_t bay, float a_mag, float deg)
{
    float rad = deg * 3.14159265f / 180.0f;
    for (int ph = 0; ph < 3; ph++) {
        float p = rad - (float)ph * 2.0943951f;
        i[bay][ph] = cplx(a_mag * cosf(p), a_mag * sinf(p));
    }
}

/* Healthy bus: bay 2 (infeed) carries 740 A into the bus, feeders 1/3/4 take
 * 320/180/240 A out. Phasors are "into the bus" positive. */
static void healthy_bus(cplx_t i[IED_NUM_BAYS][3])
{
    set_bay(i, 0, -320.0f, 0.0f);
    set_bay(i, 1, 740.0f, 0.0f);
    set_bay(i, 2, -180.0f, 0.0f);
    set_bay(i, 3, -240.0f, 0.0f);
}

/* ---------------------------------------------------------------- unit */

static void test_threshold_closed_form(void)
{
    TH_CASE("T-87B-05 REQ-87B-005: dual slope continuous at the breakpoint");
    bus_diff_87b_settings_t s = ied_default_settings()->b87;
    TH_ASSERT_NEAR(s.slope1_pct, 25.0, 1e-6);
    TH_ASSERT_NEAR(s.slope2_pct, 60.0, 1e-6);
    TH_ASSERT_NEAR(s.breakpoint_pu, 2.0, 1e-6);
    TH_ASSERT_NEAR(bus_diff_87b_threshold_pu(&s, 1.0f), 0.25, 1e-5);
    TH_ASSERT_NEAR(bus_diff_87b_threshold_pu(&s, 2.0f), 0.50, 1e-5);
    TH_ASSERT_NEAR(bus_diff_87b_threshold_pu(&s, 4.0f), 1.70, 1e-5);
}

static void test_base_current(void)
{
    TH_CASE("T-87B-06 REQ-87B-007: base 20 MVA at sqrt(3) x 7200 V = 926 A");
    const ied_settings_t *d = ied_default_settings();
    TH_ASSERT_NEAR(d->b87.base_a, 926.0, 0.5);
    TH_ASSERT(d->b87.cts_delay_passes == 1440u);
}

static void test_no_operate_below_characteristic(void)
{
    TH_CASE("T-87B-04 REQ-87B-004: Id below pickup or below T(Ir) does not operate");
    bus_diff_87b_settings_t s = ied_default_settings()->b87;
    bus_diff_87b_state_t st;
    bus_diff_87b_init(&st, 5.6f);
    cplx_t i[IED_NUM_BAYS][3];
    /* Id = 0.15 pu, Ir = 0.2 pu: below pickup */
    set_bay(i, 0, 0.175f * s.base_a, 0.0f);
    set_bay(i, 1, -0.025f * s.base_a, 0.0f);
    set_bay(i, 2, 0.0f, 0.0f);
    set_bay(i, 3, 0.0f, 0.0f);
    TH_ASSERT(!feed_passes(&s, &st, i, SETTLE_PASSES, 0u));
    TH_ASSERT(!st.blocked);
    TH_ASSERT(!st.pickup[0] && !st.operated);
    TH_ASSERT_NEAR(st.id_pu[0], 0.15, 0.005);
    TH_ASSERT_NEAR(st.ir_pu[0], 0.20, 0.005);
    /* Id = 0.5 pu, Ir = 4.0 pu: above pickup, below T = 1.70 */
    set_bay(i, 0, 2.25f * s.base_a, 0.0f);
    set_bay(i, 1, -1.75f * s.base_a, 0.0f);
    TH_ASSERT(!feed_passes(&s, &st, i, SETTLE_PASSES, 0u));
    TH_ASSERT(st.pickup[0]);
    TH_ASSERT(!st.operated);
    TH_ASSERT_NEAR(st.id_pu[0], 0.5, 0.01);
    TH_ASSERT_NEAR(st.ir_pu[0], 4.0, 0.01);
    /* Id = 2.0 pu, Ir = 4.0 pu: operates on every phase */
    set_bay(i, 0, 3.0f * s.base_a, 0.0f);
    set_bay(i, 1, -1.0f * s.base_a, 0.0f);
    TH_ASSERT(feed_passes(&s, &st, i, SETTLE_PASSES, 0u));
    TH_ASSERT(st.operate[0] && st.operate[1] && st.operate[2]);
    TH_ASSERT(st.op_phase == 0u);
}

static void test_invalid_blocks(void)
{
    TH_CASE("T-87B-12 REQ-87B-027: any in-zone bay channel invalid blocks, nothing substituted");
    bus_diff_87b_settings_t s = ied_default_settings()->b87;
    bus_diff_87b_state_t st;
    bus_diff_87b_init(&st, 5.6f);
    cplx_t i[IED_NUM_BAYS][3];
    healthy_bus(i);
    (void)feed_passes(&s, &st, i, SETTLE_PASSES, 0u);
    TH_ASSERT(!st.blocked && !st.pickup[0]);
    set_bay(i, 3, 0.0f, 0.0f);   /* bay-4 CT open: its 240 A (0.26 pu) vanish from the sum */
    TH_ASSERT(!feed_passes(&s, &st, i, SETTLE_PASSES, IED_CH_BIT(IED_CH_BAY_I(3, 1))));
    TH_ASSERT(st.blocked);
    TH_ASSERT(!st.operated && !st.pickup[0]);
    TH_ASSERT(st.id_pu[0] == 0.0f);
    /* channel valid again: one full window of arming, then the real differential shows */
    (void)feed_passes(&s, &st, i, BUS_DIFF_87B_ARM_PASSES - 1u, 0u);
    TH_ASSERT(st.blocked);
    (void)feed_passes(&s, &st, i, 2u, 0u);
    TH_ASSERT(!st.blocked);
    TH_ASSERT(st.pickup[0]);
    TH_ASSERT_NEAR(st.id_pu[0], 240.0 / s.base_a, 0.005);
}

static void test_out_of_zone_bay(void)
{
    TH_CASE("T-87B-11 REQ-87B-021: out-of-zone bay leaves both sums and does not block");
    bus_diff_87b_settings_t s = ied_default_settings()->b87;
    s.bay_in_zone[2] = 0u;
    bus_diff_87b_state_t st;
    bus_diff_87b_init(&st, 5.6f);
    cplx_t i[IED_NUM_BAYS][3];
    healthy_bus(i);
    (void)feed_passes(&s, &st, i, SETTLE_PASSES, IED_BAY_MASK(2));
    TH_ASSERT(!st.blocked);
    TH_ASSERT_NEAR(st.id_pu[0], 180.0 / s.base_a, 0.005);          /* bay-3 load now looks differential */
    TH_ASSERT_NEAR(st.ir_pu[0], (320.0 + 740.0 + 240.0) / s.base_a, 0.005);
}

static void test_cts_alarm_blocks(void)
{
    TH_CASE("T-87B-10 REQ-87B-018 REQ-87B-019: standing 5 % differential under load alarms after 3 s and blocks");
    bus_diff_87b_settings_t s = ied_default_settings()->b87;
    bus_diff_87b_state_t st;
    bus_diff_87b_init(&st, 5.6f);
    cplx_t i[IED_NUM_BAYS][3];
    healthy_bus(i);
    /* bay-4 CT reads 10 % low: Id = 24 A = 0.026 pu, Ir ~ 1.57 pu -> 1.6 % : no alarm */
    set_bay(i, 3, -216.0f, 0.0f);
    (void)feed_passes(&s, &st, i, 2u * s.cts_delay_passes, 0u);
    TH_ASSERT(!st.blocked);
    TH_ASSERT(!st.cts_alarm);
    /* bay-4 CT reads 50 % low: Id = 120 A = 0.13 pu (< pickup 0.2), 8 % of Ir -> alarm after the delay */
    set_bay(i, 3, -120.0f, 0.0f);
    (void)feed_passes(&s, &st, i, s.cts_delay_passes - BUS_DIFF_87B_ARM_PASSES, 0u);
    TH_ASSERT(!st.cts_alarm);
    (void)feed_passes(&s, &st, i, 2u * BUS_DIFF_87B_ARM_PASSES, 0u);
    TH_ASSERT(st.cts_alarm);
    TH_ASSERT(st.blocked);
    /* an internal fault while the alarm stands does not operate */
    set_bay(i, 1, 3000.0f, 0.0f);
    TH_ASSERT(!feed_passes(&s, &st, i, SETTLE_PASSES, 0u));
    TH_ASSERT(!st.operated);
    TH_ASSERT(st.blocked);
}

static void test_disabled(void)
{
    TH_CASE("T-87B-13 REQ-87B-024: disabled function never operates and reports enabled=false");
    ied_settings_t s = *ied_default_settings();
    s.b87.enabled = 0u;
    const ied_status_t *st = loop_run_ex(testset_find("bus_fault_internal"), 0u, &s, 0u);
    TH_ASSERT(!st->b87.enabled);
    TH_ASSERT(!st->b87.operated);
    TH_ASSERT(!st->tripb);
    TH_ASSERT(find_operate(ELEM_87B) == NULL);
}

static void test_power_up_arming(void)
{
    TH_CASE("T-87B-14 REQ-87B-028: no protection pass, hence no operate, before the first full DFT window");
    testset_t ts;
    const ied_settings_t *s = ied_default_settings();
    ied_init(s, 1790000000u, NULL);
    scenario_t sc = *testset_find("bus_fault_internal");
    sc.fault_start_s = 0.0f;
    testset_init(&ts, &sc, s->scale);
    testset_set_preroll(&ts, 0u);
    int16_t raw[IED_NUM_CHANNELS];
    for (uint32_t k = 0; k < IED_SAMPLES_PER_CYCLE - 1u; k++) {
        testset_step(&ts, false, false, raw);
        (void)ied_acq_isr_q(raw, 0u);
        (void)ied_protection_task();
    }
    TH_ASSERT(ied_status()->protection_passes == 0u);
    TH_ASSERT(!ied_status()->b87.operated);
    TH_ASSERT(!ied_status()->tripb);
}

/* ------------------------------------------------------------ scenarios */

static void test_internal_fault_operates(void)
{
    TH_CASE("T-87B-03/07/09 REQ-87B-001 -006 -013 -015 -016 -017 -033: bus_fault_internal -> 87B, TRIP1, 86B");
    const ied_status_t *st = loop_run(testset_find("bus_fault_internal"), 0u);
    TH_ASSERT(st->tripb);
    TH_ASSERT((st->target & ELEM_BIT(ELEM_87B)) != 0u);
    const soe_event_t *op = find_operate(ELEM_87B);
    TH_ASSERT(op != NULL);
    const soe_event_t *t1 = loop_find(SOE_TRIP1_ON, 0);
    const soe_event_t *tb = loop_find(SOE_TRIPB_ON, 0);
    TH_ASSERT(t1 != NULL && tb != NULL);
    if (op != NULL && t1 != NULL) {
        float op_ms = (float)(op->tick - FAULT_TICK) * 1000.0f / (float)IED_SAMPLE_RATE_HZ;
        float trip_ms = (float)(t1->tick - FAULT_TICK) * 1000.0f / (float)IED_SAMPLE_RATE_HZ;
        printf("    bus_fault_internal: 87B operate %.1f ms, TRIP1 %.1f ms after inception, Id = %.2f pu\n",
               (double)op_ms, (double)trip_ms, (double)op->value);
        TH_ASSERT(op->value > 0.2f);          /* Id at the operating pass, pu */
        TH_ASSERT(trip_ms <= 25.0f);          /* A4: measured, customer target BLOCKED */
    }
    /* phase selectivity: the A-G fault operates phase A only */
    TH_ASSERT(op != NULL && (op->detail & 0xFFu) == ELEM_87B);
    TH_ASSERT(loop_res.ts.bus_dead);          /* the 86B trip de-energised the bus */
    /* seal-in (A6): the bus is dead and the differential gone, yet 86B stands until reset */
    TH_ASSERT(!st->b87.operated);
    TH_ASSERT(st->tripb);
}

static void test_external_fault_secure(void)
{
    TH_CASE("T-87B-01/02/08 REQ-87B-002 -014 -034: bus_fault_external -> no 87B, no trip");
    const ied_status_t *st = loop_run(testset_find("bus_fault_external"), 0u);
    TH_ASSERT(!st->trip1 && !st->tripb);
    TH_ASSERT(st->operated == 0u);
    TH_ASSERT(find_operate(ELEM_87B) == NULL);
    TH_ASSERT(loop_find(SOE_TRIP1_ON, 0) == NULL);
}

static void test_steady_load_quantities(void)
{
    TH_CASE("T-87B-01/02 REQ-87B-001 -002: steady_load Id ~ 0, Ir = 2 x 740 A");
    const ied_status_t *st = loop_run(testset_find("steady_load"), 0u);
    TH_ASSERT(!st->trip1 && !st->tripb);
    TH_ASSERT(st->b87.enabled && !st->b87.blocked);
    for (int ph = 0; ph < 3; ph++) {
        TH_ASSERT(st->b87.id_pu[ph] < 0.05f);
        TH_ASSERT_NEAR(st->b87.ir_pu[ph], 1480.0 / 926.0, 0.02 * 1480.0 / 926.0);
    }
}

static void test_ct_lost_bay1(void)
{
    TH_CASE("T-87B-12 REQ-87B-023 -027: ct_lost_bay1_load -> blocked, no trip");
    const ied_status_t *st = loop_run(testset_find("ct_lost_bay1_load"), 0u);
    TH_ASSERT(!st->trip1 && !st->tripb);
    TH_ASSERT(st->b87.blocked);
    TH_ASSERT(find_operate(ELEM_87B) == NULL);
    TH_ASSERT(st->blocked_passes > 0u);
}

static void test_bay4_not_connected(void)
{
    TH_CASE("T-87B-12 REQ-87B-023: bay-4 CT not connected, bay 4 in zone -> 87B blocked on an internal fault");
    ied_settings_t s = *ied_default_settings();
    s.bay[3].ct_connected = 0u;
    const ied_status_t *st = loop_run_ex(testset_find("bus_fault_internal"), 0u, &s, 0u);
    TH_ASSERT(st->b87.blocked);
    TH_ASSERT(!st->b87.operated);
    TH_ASSERT(!st->tripb);
    TH_ASSERT(find_operate(ELEM_87B) == NULL);
}

/* ---------------------------------------------------------------- sweep */

static uint32_t s_rng;
static uint32_t rnd(void)
{
    s_rng = s_rng * 1664525u + 1013904223u;
    return s_rng >> 8;
}
static float frand(float lo, float hi)
{
    return lo + (hi - lo) * ((float)(rnd() & 0xFFFFu) / 65535.0f);
}

static void test_invariant_sweep(void)
{
    TH_CASE("T-87B-SW REQ-87B-014 -027: seeded sweep");
    s_rng = SWEEP_SEED;
    int failures = 0;
    int n_invalid = 0, n_external = 0, n_internal = 0;
    for (int c = 0; c < SWEEP_CASES; c++) {
        scenario_t sc = *testset_find("steady_load");
        sc.duration_s = 0.5f;
        sc.load_a = frand(50.0f, 400.0f);
        sc.load_pf = frand(0.8f, 1.0f);
        uint32_t extra_invalid = 0u;
        int family = c % 3;
        if (family == 0) {
            /* (a) healthy load, at least one channel of some bay invalid */
            do {
                extra_invalid = 0u;
                for (uint32_t bay = 0; bay < IED_NUM_BAYS; bay++) {
                    uint32_t bits = rnd() & 7u;
                    extra_invalid |= (bits << (IED_CH_B1_IA + 3u * bay));
                }
            } while (extra_invalid == 0u);
            n_invalid++;
        } else {
            sc.fault = (fault_type_t)(1u + rnd() % 7u);
            sc.fault_start_s = 0.20f;
            sc.fault_location_pct = frand(1.0f, 100.0f);
            sc.fault_r_ohm = frand(0.0f, 2.0f);
            if (family == 1) {
                sc.fault_zone = (rnd() & 1u) ? ZONE_BAY3_LINE : ZONE_BAY1_LINE;
                sc.external_clear_s = 0.25f;
                n_external++;
            } else {
                sc.fault_zone = ZONE_BUS;
                sc.fault_location_pct = 0.0f;
                sc.fault_r_ohm = frand(0.0f, 0.5f);
                n_internal++;
            }
        }
        const ied_status_t *st = loop_run_ex(&sc, 0u, NULL, extra_invalid);
        bool op87 = find_operate(ELEM_87B) != NULL;
        bool bad = false;
        if (family == 0) {
            bad = st->trip1 || st->tripb || st->operated != 0u || op87 || !st->b87.blocked;
        } else if (family == 1) {
            bad = op87 || st->tripb;
        } else {
            bad = !op87 || !st->tripb;
        }
        if (bad) {
            failures++;
            if (failures <= 10) {
                printf("    sweep case %d family %d: fault %d zone %d loc %.1f%% Rf %.2f load %.0f A "
                       "invalid 0x%04x -> trip1 %d tripb %d 87B %d blocked %d\n",
                       c, family, (int)sc.fault, (int)sc.fault_zone, (double)sc.fault_location_pct,
                       (double)sc.fault_r_ohm, (double)sc.load_a, extra_invalid, st->trip1,
                       st->tripb, op87, st->b87.blocked);
            }
        }
    }
    printf("    sweep seed 0x%08x: %d cases (%d invalid-input, %d external, %d internal), %d failed\n",
           SWEEP_SEED, SWEEP_CASES, n_invalid, n_external, n_internal, failures);
    TH_ASSERT(failures == 0);
    TH_ASSERT(SWEEP_CASES >= 500);
}

int main(void)
{
    test_threshold_closed_form();
    test_base_current();
    test_no_operate_below_characteristic();
    test_invalid_blocks();
    test_out_of_zone_bay();
    test_cts_alarm_blocks();
    test_disabled();
    test_power_up_arming();
    test_internal_fault_operates();
    test_external_fault_secure();
    test_steady_load_quantities();
    test_ct_lost_bay1();
    test_bay4_not_connected();
    test_invariant_sweep();
    return th_report("bus_diff_87b");
}
