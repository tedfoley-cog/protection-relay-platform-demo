/* Settings file + shared rules through the firmware's own parser (the same
 * path the protocol write uses). */
#include "test_harness.h"
#include "settings_file.h"

#include <string.h>

static bool has_rule(const settings_result_t *r, const char *id)
{
    for (int k = 0; k < r->count && k < SETTINGS_MAX_ERRORS; k++) {
        if (strcmp(r->err[k].key, id) == 0) {
            return true;
        }
    }
    return false;
}

static void test_defaults_valid(void)
{
    TH_CASE("default group round-trips through the file format and passes every rule");
    static char buf[8192];
    const ied_settings_t *d = ied_default_settings();
    size_t n = settings_file_write(d, buf, sizeof(buf));
    TH_ASSERT(n > 100u);
    ied_settings_t out;
    settings_result_t res;
    TH_ASSERT(settings_file_parse(buf, d, &out, &res));
    TH_ASSERT(res.count == 0);
    TH_ASSERT(out.t51p.curve == TOC_IEEE_VI);
    TH_ASSERT_NEAR(out.scale[IED_CH_B3_IA], 0.5, 1e-6);
}

static void test_tms_out_of_range_rejected(void)
{
    TH_CASE("51P TMS 0.01 is rejected by R-51-002 and the group is not applied");
    ied_settings_t out;
    settings_result_t res;
    memset(&out, 0, sizeof(out));
    bool ok = settings_file_parse("51P_TMS = 0.01\n", ied_default_settings(), &out, &res);
    TH_ASSERT(!ok);
    TH_ASSERT(has_rule(&res, "R-51-002"));
    TH_ASSERT(out.t51p.pickup_a == 0.0f);   /* untouched */
}

static void test_zone_order_rejected(void)
{
    TH_CASE("zone 2 reach below zone 1 is rejected by R-21-003");
    ied_settings_t out;
    settings_result_t res;
    bool ok = settings_file_parse("21_Z2_REACH_PCT = 70\n", ied_default_settings(), &out, &res);
    TH_ASSERT(!ok);
    TH_ASSERT(has_rule(&res, "R-21-003"));
    TH_ASSERT(has_rule(&res, "R-21-002"));
}

static void test_unknown_key_and_bad_curve(void)
{
    TH_CASE("unknown keys and unknown curve names reject the write");
    ied_settings_t out;
    settings_result_t res;
    TH_ASSERT(!settings_file_parse("87B_SLOPE1 = 0.3\n", ied_default_settings(), &out, &res));
    TH_ASSERT(res.err[0].line == 1);
    TH_ASSERT(!settings_file_parse("51P_CURVE = IEC_WEIRD\n", ied_default_settings(), &out, &res));
}

static void test_ct_rules(void)
{
    TH_CASE("bay CT ratio rules: primary 0 and secondary 2 A rejected");
    ied_settings_t out;
    settings_result_t res;
    bool ok = settings_file_parse("BAY3_CT_PRIMARY_A = 0\nBAY4_CT_SECONDARY_A = 2\n",
                                  ied_default_settings(), &out, &res);
    TH_ASSERT(!ok);
    TH_ASSERT(has_rule(&res, "BAY3"));
    TH_ASSERT(has_rule(&res, "BAY4"));
}

int main(void)
{
    test_defaults_valid();
    test_tms_out_of_range_rejected();
    test_zone_order_rejected();
    test_unknown_key_and_bad_curve();
    test_ct_rules();
    return th_report("settings_rules");
}
