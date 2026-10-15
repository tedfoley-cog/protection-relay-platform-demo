/* The firmware links the same shared math the setting tool runs as WASM.
 * Pin a few published-standard points here; the full vector file
 * (shared/relay_math/vectors/parity_vectors.json) is replayed by the tool. */
#include "test_harness.h"
#include "oc_element.h"
#include "pu_scale.h"

#include <math.h>

static void test_iec_points(void)
{
    TH_CASE("IEC 60255-151 SI/VI/EI at 2x and 10x pickup");
    TH_ASSERT_NEAR(toc_curve_operate_time(TOC_IEC_SI, 1.0f, 0.0f, 10.0f), 0.14 / (pow(10, 0.02) - 1), 1e-4);
    TH_ASSERT_NEAR(toc_curve_operate_time(TOC_IEC_VI, 1.0f, 0.0f, 2.0f), 13.5, 1e-4);
    TH_ASSERT_NEAR(toc_curve_operate_time(TOC_IEC_EI, 1.0f, 0.0f, 2.0f), 80.0 / 3.0, 1e-4);
}

static void test_ieee_points(void)
{
    TH_CASE("IEEE C37.112 VI operate and reset");
    TH_ASSERT_NEAR(toc_curve_operate_time(TOC_IEEE_VI, 2.0f, 0.0f, 5.0f), 2.0 * (19.61 / 24.0 + 0.491), 1e-4);
    TH_ASSERT_NEAR(toc_curve_reset_time(TOC_IEEE_VI, 1.0f, 0.5f), 21.6 / 0.75, 1e-4);
    TH_ASSERT(toc_curve_operate_time(TOC_IEEE_VI, 1.0f, 0.0f, 0.9f) < 0.0f);
    TH_ASSERT_NEAR(toc_curve_operate_time(TOC_DEFINITE, 1.0f, 0.35f, 3.0f), 0.35, 1e-6);
}

static void test_element_wrapper_matches_shared(void)
{
    TH_CASE("51 element wrapper returns the shared curve value");
    toc_settings_t s = {600.0f, 0.5f, TOC_IEEE_VI, 0.0f};
    TH_ASSERT_NEAR(toc_operate_time(&s, 3.0f), toc_curve_operate_time(TOC_IEEE_VI, 0.5f, 0.0f, 3.0f), 0.0);
}

static void test_pu_helpers(void)
{
    TH_CASE("per-unit helpers");
    TH_ASSERT_NEAR(pu_base_a_from_mva(20.0f, 12.47f), 20e6 / (sqrt(3.0) * 12470.0), 0.01);
    TH_ASSERT_NEAR(pu_a_per_count(2000.0f, 1200.0f), 2000.0 / 1200.0, 1e-6);
    TH_ASSERT_NEAR(pu_secondary_a(3000.0f, 600.0f, 5.0f), 25.0, 1e-5);
    TH_ASSERT(pu_from_a(100.0f, 0.0f) == 0.0f);
}

int main(void)
{
    test_iec_points();
    test_ieee_points();
    test_element_wrapper_matches_shared();
    test_pu_helpers();
    return th_report("shared_parity");
}
