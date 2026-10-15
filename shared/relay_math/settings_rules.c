#include "settings_rules.h"

#include "toc_curve.h"

#include <stddef.h>

#define F_NOMINAL_HZ 60.0f

typedef struct {
    const char *id;
    const char *message;
} rule_text_t;

static const rule_text_t k_text[RULE_COUNT] = {
    {"R-51-001", "51 pickup must be 10..20000 A primary"},
    {"R-51-002", "51 time multiplier must be 0.05..15.0"},
    {"R-51-003", "51 curve must be one of IEC_SI IEC_VI IEC_EI IEC_LTI IEEE_MI IEEE_VI IEEE_EI DEFINITE"},
    {"R-51-004", "51 definite time must be 0.00..100.00 s"},
    {"R-51-005", "51P pickup must be 0.1..20 x bay-1 CT rated primary"},
    {"R-50-001", "50 pickup must not be below the 51 pickup of the same measurand"},
    {"R-21-001", "21 zone 1 reach must be 10..90 % of the line"},
    {"R-21-002", "21 zone 2 reach must be 100..250 % of the line"},
    {"R-21-003", "21 zone 1 reach must be below zone 2 reach"},
    {"R-21-004", "21 zone 2 delay must be 0.10..5.00 s"},
    {"R-21-005", "21 minimum loop current must be 20..2000 A"},
    {"R-BF-001", "50BF current detector must be 20..5000 A"},
    {"R-BF-002", "50BF retrip delay must be below the breaker-failure delay"},
    {"R-81-001", "81U pickup must be below nominal frequency and 81O above it"},
    {"R-81-002", "81R pickup must be 0.1..10.0 Hz/s"},
    {"R-VT-001", "VT nominal phase-neutral voltage must be 50..500000 V"},
    {"R-CT-001", "bay CT rated primary must be 1..20000 A"},
    {"R-CT-002", "bay CT rated secondary must be 1 A or 5 A"},
};

static const char *const k_input_names[RULES_IN_COUNT] = {
    "51P_PICKUP_A", "51P_TMS", "51P_CURVE", "51P_DEFINITE_S",
    "51G_PICKUP_A", "51G_TMS", "51G_CURVE", "51G_DEFINITE_S",
    "50P_PICKUP_A", "50G_PICKUP_A",
    "21_Z1_REACH_PCT", "21_Z2_REACH_PCT", "21_Z2_DELAY_S", "21_MIN_LOOP_A",
    "50BF_PICKUP_A", "50BF_RETRIP_S", "50BF_DELAY_S",
    "81U_PICKUP_HZ", "81O_PICKUP_HZ", "81R_PICKUP_HZ_S",
    "VT_V_LN_NOMINAL",
    "BAY1_CT_PRIMARY_A", "BAY1_CT_SECONDARY_A", "BAY2_CT_PRIMARY_A", "BAY2_CT_SECONDARY_A",
    "BAY3_CT_PRIMARY_A", "BAY3_CT_SECONDARY_A", "BAY4_CT_PRIMARY_A", "BAY4_CT_SECONDARY_A",
};

static int add(rule_violation_t *out, int max, int n, int code, int detail, float value)
{
    if (out != NULL && n < max) {
        out[n].code = code;
        out[n].detail = detail;
        out[n].value = value;
    }
    return n + 1;
}

static int in_range(float v, float lo, float hi)
{
    return v >= lo && v <= hi;
}

static int check_toc(const float in[], int base, rule_violation_t *out, int max, int n)
{
    float pickup = in[base + 0];
    float tms = in[base + 1];
    float curve = in[base + 2];
    float definite = in[base + 3];
    if (!in_range(pickup, 10.0f, 20000.0f)) {
        n = add(out, max, n, RULE_51_PICKUP_RANGE, base, pickup);
    }
    if (!in_range(tms, 0.05f, 15.0f)) {
        n = add(out, max, n, RULE_51_TMS_RANGE, base, tms);
    }
    if (!(curve >= 0.0f && curve < (float)TOC_CURVE_COUNT && curve == (float)(int)curve)) {
        n = add(out, max, n, RULE_51_CURVE_VALID, base, curve);
    } else if ((int)curve == (int)TOC_DEFINITE && !in_range(definite, 0.0f, 100.0f)) {
        n = add(out, max, n, RULE_51_DEFINITE_RANGE, base, definite);
    }
    return n;
}

int settings_rules_check(const float in[RULES_IN_COUNT], rule_violation_t *out, int max)
{
    int n = 0;
    if (in == NULL) {
        return 0;
    }
    n = check_toc(in, RULES_IN_51P_PICKUP_A, out, max, n);
    n = check_toc(in, RULES_IN_51G_PICKUP_A, out, max, n);

    float ct1 = in[RULES_IN_BAY1_CT_PRIMARY_A];
    if (ct1 > 0.0f && !in_range(in[RULES_IN_51P_PICKUP_A], 0.1f * ct1, 20.0f * ct1)) {
        n = add(out, max, n, RULE_51_PICKUP_VS_CT, 0, in[RULES_IN_51P_PICKUP_A]);
    }
    if (in[RULES_IN_50P_PICKUP_A] < in[RULES_IN_51P_PICKUP_A]) {
        n = add(out, max, n, RULE_50_ABOVE_51, 0, in[RULES_IN_50P_PICKUP_A]);
    }
    if (in[RULES_IN_50G_PICKUP_A] < in[RULES_IN_51G_PICKUP_A]) {
        n = add(out, max, n, RULE_50_ABOVE_51, 1, in[RULES_IN_50G_PICKUP_A]);
    }

    float z1 = in[RULES_IN_21_Z1_REACH_PCT];
    float z2 = in[RULES_IN_21_Z2_REACH_PCT];
    if (!in_range(z1, 10.0f, 90.0f)) {
        n = add(out, max, n, RULE_21_Z1_RANGE, 0, z1);
    }
    if (!in_range(z2, 100.0f, 250.0f)) {
        n = add(out, max, n, RULE_21_Z2_RANGE, 0, z2);
    }
    if (z1 >= z2) {
        n = add(out, max, n, RULE_21_ZONE_ORDER, 0, z1);
    }
    if (!in_range(in[RULES_IN_21_Z2_DELAY_S], 0.10f, 5.0f)) {
        n = add(out, max, n, RULE_21_Z2_DELAY_RANGE, 0, in[RULES_IN_21_Z2_DELAY_S]);
    }
    if (!in_range(in[RULES_IN_21_MIN_LOOP_A], 20.0f, 2000.0f)) {
        n = add(out, max, n, RULE_21_MIN_LOOP_RANGE, 0, in[RULES_IN_21_MIN_LOOP_A]);
    }

    if (!in_range(in[RULES_IN_50BF_PICKUP_A], 20.0f, 5000.0f)) {
        n = add(out, max, n, RULE_50BF_PICKUP_RANGE, 0, in[RULES_IN_50BF_PICKUP_A]);
    }
    if (in[RULES_IN_50BF_RETRIP_S] >= in[RULES_IN_50BF_DELAY_S]) {
        n = add(out, max, n, RULE_50BF_TIMER_ORDER, 0, in[RULES_IN_50BF_RETRIP_S]);
    }

    if (!(in[RULES_IN_81U_PICKUP_HZ] < F_NOMINAL_HZ && in[RULES_IN_81O_PICKUP_HZ] > F_NOMINAL_HZ)) {
        n = add(out, max, n, RULE_81_ORDER, 0, in[RULES_IN_81U_PICKUP_HZ]);
    }
    if (!in_range(in[RULES_IN_81R_PICKUP_HZ_S], 0.1f, 10.0f)) {
        n = add(out, max, n, RULE_81_ROCOF_RANGE, 0, in[RULES_IN_81R_PICKUP_HZ_S]);
    }
    if (!in_range(in[RULES_IN_VT_V_LN_NOMINAL], 50.0f, 500000.0f)) {
        n = add(out, max, n, RULE_VT_NOMINAL_RANGE, 0, in[RULES_IN_VT_V_LN_NOMINAL]);
    }
    for (int bay = 0; bay < 4; bay++) {
        float prim = in[RULES_IN_BAY1_CT_PRIMARY_A + 2 * bay];
        float sec = in[RULES_IN_BAY1_CT_SECONDARY_A + 2 * bay];
        if (!in_range(prim, 1.0f, 20000.0f)) {
            n = add(out, max, n, RULE_CT_PRIMARY_RANGE, bay, prim);
        }
        if (!(sec == 1.0f || sec == 5.0f)) {
            n = add(out, max, n, RULE_CT_SECONDARY_VALUE, bay, sec);
        }
    }
    return n;
}

const char *settings_rule_id(int code)
{
    return (code >= 0 && code < RULE_COUNT) ? k_text[code].id : "R-XX-000";
}

const char *settings_rule_message(int code)
{
    return (code >= 0 && code < RULE_COUNT) ? k_text[code].message : "unknown rule";
}

const char *settings_rules_input_name(int index)
{
    return (index >= 0 && index < RULES_IN_COUNT) ? k_input_names[index] : "";
}
