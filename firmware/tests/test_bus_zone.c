/* Four-bay measurement chain: the test set obeys Kirchhoff at the bus for
 * every scenario and the relay's bay table matches the plant. These tests pin
 * the substrate the 87B bus element reads. */
#include "test_harness.h"
#include "ied_loop.h"

static cplx_t bus_sum(const testset_t *ts, int ph)
{
    cplx_t sum = cplx(0.0f, 0.0f);
    for (uint32_t bay = 0; bay < IED_NUM_BAYS; bay++) {
        sum = cplx_add(sum, testset_bay_current(ts, bay, ph));
    }
    return sum;   /* current out of the bus; zero unless the bus itself is faulted */
}

static void run_to(testset_t *ts, const scenario_t *sc, float t_s)
{
    const ied_settings_t *s = ied_default_settings();
    testset_init(ts, sc, s->scale);
    int16_t raw[IED_NUM_CHANNELS];
    while (testset_time_s(ts) < t_s) {
        testset_step(ts, false, false, raw);
    }
}

static void test_external_kcl(void)
{
    TH_CASE("external fault on bay 3: bay currents sum to zero at the bus");
    testset_t ts;
    run_to(&ts, testset_find("bus_fault_external"), 0.35f);
    TH_ASSERT(ts.fault_active);
    for (int ph = 0; ph < 3; ph++) {
        TH_ASSERT_NEAR(cplx_abs(bus_sum(&ts, ph)), 0.0, 0.5);
    }
    TH_ASSERT(cplx_abs(testset_bay_current(&ts, 2, 1)) > 2500.0f);   /* bay 3 phase B carries the fault */
    TH_ASSERT(cplx_abs(testset_bay_current(&ts, 1, 1)) > 2500.0f);   /* supplied by the infeed */
}

static void test_internal_kcl(void)
{
    TH_CASE("bus fault: sum of bay currents equals the fault current");
    testset_t ts;
    run_to(&ts, testset_find("bus_fault_internal"), 0.35f);
    TH_ASSERT(ts.fault_active);
    float i_fault = cplx_abs(bus_sum(&ts, 0));
    TH_ASSERT(i_fault > 2000.0f);
    TH_ASSERT_NEAR(cplx_abs(bus_sum(&ts, 1)), 0.0, 1.0);
    TH_ASSERT_NEAR(cplx_abs(bus_sum(&ts, 2)), 0.0, 1.0);
}

static void test_feeder_fault_is_external(void)
{
    TH_CASE("bay-1 line fault is external to the bus zone");
    testset_t ts;
    run_to(&ts, testset_find("fault_ag_zone1"), 0.35f);
    for (int ph = 0; ph < 3; ph++) {
        TH_ASSERT_NEAR(cplx_abs(bus_sum(&ts, ph)), 0.0, 0.5);
    }
}

static void test_bay_table_matches_plant(void)
{
    TH_CASE("relay bay table matches the test set's CTs and scales by CT ratio");
    const ied_settings_t *s = ied_default_settings();
    const bay_config_t *plant = testset_default_bays();
    for (uint32_t bay = 0; bay < IED_NUM_BAYS; bay++) {
        TH_ASSERT(s->bay[bay].ct_primary_a == plant[bay].ct_primary_a);
        TH_ASSERT(s->bay[bay].polarity == plant[bay].polarity);
        float a_per_count = plant[bay].ct_primary_a / IED_ADC_COUNTS_PER_CT_RATED;
        for (uint32_t ph = 0; ph < 3; ph++) {
            TH_ASSERT_NEAR(s->scale[IED_CH_BAY_I(bay, ph)], a_per_count, 1e-6);
        }
    }
    TH_ASSERT_NEAR(s->scale[IED_CH_B2_IA], 2000.0 / 1200.0, 1e-6);
}

static void test_only_87b_operates_on_bus_fault(void)
{
    TH_CASE("only 87B operates for an internal bus fault; no feeder element reaches the bus");
    const ied_status_t *st = loop_run(testset_find("bus_fault_internal"), 0u);
    TH_ASSERT(st->tripb);
    TH_ASSERT((st->target & ~ELEM_BIT(ELEM_87B)) == 0u);
}

int main(void)
{
    test_external_kcl();
    test_internal_kcl();
    test_feeder_fault_is_external();
    test_bay_table_matches_plant();
    test_only_87b_operates_on_bus_fault();
    return th_report("bus_zone");
}
