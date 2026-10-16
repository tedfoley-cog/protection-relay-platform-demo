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
    TH_ASSERT(!settings_file_parse("99X_NOT_A_KEY = 0.3\n", ied_default_settings(), &out, &res));
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

/* 87B shared rules R-87B-001..009 through the firmware parser.
 * VALIDATION_PLAN T-87B-R01..R09, T-87B-15; REQ-87B-008 REQ-87B-009 REQ-87B-010
 * REQ-87B-011 REQ-87B-012 REQ-87B-022 REQ-87B-025 REQ-87B-026 REQ-87B-029 REQ-87B-030. */
static void expect_rule(const char *text, const char *rule)
{
    ied_settings_t out;
    settings_result_t res;
    memset(&out, 0, sizeof(out));
    bool ok = settings_file_parse(text, ied_default_settings(), &out, &res);
    TH_ASSERT(!ok);
    TH_ASSERT(has_rule(&res, rule));
    TH_ASSERT(out.b87.pickup_pu == 0.0f);   /* rejected group leaves the target untouched */
}

static void test_87b_rules(void)
{
    TH_CASE("87B settings outside their ranges or with slope 2 below slope 1 are rejected");
    expect_rule("87B_PICKUP = 0.04\n", "R-87B-001");
    expect_rule("87B_PICKUP = 2.1\n", "R-87B-001");
    expect_rule("87B_SLOPE1 = 5\n", "R-87B-002");
    expect_rule("87B_SLOPE2 = 101\n", "R-87B-003");
    expect_rule("87B_BREAKPOINT = 0.9\n", "R-87B-004");
    expect_rule("87B_BREAKPOINT = 10.1\n", "R-87B-004");
    expect_rule("87B_SLOPE1 = 60\n87B_SLOPE2 = 25\n", "R-87B-005");   /* customer defaults, BLOCKED row */
    expect_rule("87B_CTS_ALARM_PCT = 1\n", "R-87B-006");
    expect_rule("87B_CTS_ALARM_PCT = 21\n", "R-87B-006");
    expect_rule("87B_CTS_DELAY = 0.4\n", "R-87B-007");
    expect_rule("87B_CTS_DELAY = 11\n", "R-87B-007");
    expect_rule("87B_BAY2_IN_ZONE = NO\n87B_BAY3_IN_ZONE = NO\n87B_BAY4_IN_ZONE = NO\n", "R-87B-008");
    expect_rule("87B_BASE_MVA = 0.5\n", "R-87B-009");
    expect_rule("87B_BASE_MVA = 1001\n", "R-87B-009");
}

static void test_87b_keys_round_trip(void)
{
    TH_CASE("87B keys parse, write back and validate as a group");
    ied_settings_t out;
    settings_result_t res;
    TH_ASSERT(!settings_file_parse("87B_FUNCTION = MAYBE\n", ied_default_settings(), &out, &res));
    TH_ASSERT(res.err[0].line == 1);
    TH_ASSERT(!settings_file_parse("87B_BAY1_IN_ZONE = TRUE\n", ied_default_settings(), &out, &res));
    bool ok = settings_file_parse("87B_FUNCTION = DISABLED\n87B_SLOPE1 = 60\n87B_SLOPE2 = 70\n87B_BAY4_IN_ZONE = NO\n",
                                  ied_default_settings(), &out, &res);
    TH_ASSERT(ok);
    TH_ASSERT(res.count == 0);
    TH_ASSERT(out.b87.enabled == 0u);
    TH_ASSERT(out.b87.slope1_pct == 60.0f && out.b87.slope2_pct == 70.0f);
    TH_ASSERT(out.b87.bay_in_zone[3] == 0u && out.b87.bay_in_zone[0] == 1u);
    TH_ASSERT_NEAR(out.b87.base_a, 926.0, 0.5);
    static char buf[8192];
    size_t n = settings_file_write(&out, buf, sizeof(buf));
    TH_ASSERT(n > 100u);
    TH_ASSERT(strstr(buf, "87B_FUNCTION = DISABLED\n") != NULL);
    TH_ASSERT(strstr(buf, "87B_SLOPE2 = 70\n") != NULL);
    TH_ASSERT(strstr(buf, "87B_BAY4_IN_ZONE = NO\n") != NULL);
}

int main(void)
{
    test_87b_rules();
    test_87b_keys_round_trip();
    test_defaults_valid();
    test_tms_out_of_range_rejected();
    test_zone_order_rejected();
    test_unknown_key_and_bad_curve();
    test_ct_rules();
    return th_report("settings_rules");
}
