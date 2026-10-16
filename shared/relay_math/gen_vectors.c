/* Emits vectors/parity_vectors.json from the C implementation. The setting
 * tool replays the file against the WASM build (tool/src/lib/relayMath.test.ts)
 * and the firmware replays it against its own link (firmware/tests/test_shared_parity.c),
 * so all three sides are pinned to the same numbers. */
#include "pu_scale.h"
#include "settings_rules.h"
#include "toc_curve.h"

#include <stdio.h>

static const float k_multiples[] = {1.05f, 1.5f, 2.0f, 3.0f, 5.0f, 10.0f, 20.0f, 30.0f};
static const float k_tms[] = {0.05f, 0.5f, 1.0f, 2.0f};

int main(void)
{
    printf("{\n  \"toc_operate\": [\n");
    int first = 1;
    for (int c = 0; c < (int)TOC_CURVE_COUNT; c++) {
        for (unsigned t = 0; t < sizeof(k_tms) / sizeof(k_tms[0]); t++) {
            for (unsigned m = 0; m < sizeof(k_multiples) / sizeof(k_multiples[0]); m++) {
                float def = 0.35f;
                float top = toc_curve_operate_time((toc_curve_t)c, k_tms[t], def, k_multiples[m]);
                float trs = toc_curve_reset_time((toc_curve_t)c, k_tms[t], 0.5f);
                printf("%s    {\"curve\": \"%s\", \"tms\": %.4g, \"definite_s\": %.4g, "
                       "\"multiple\": %.4g, \"operate_s\": %.9g, \"reset_s_at_0p5\": %.9g}",
                       first ? "" : ",\n", toc_curve_name((toc_curve_t)c), (double)k_tms[t],
                       (double)def, (double)k_multiples[m], (double)top, (double)trs);
                first = 0;
            }
        }
    }
    printf("\n  ],\n  \"pu_scale\": [\n");
    const float ct[4][2] = {{1200.0f, 5.0f}, {2000.0f, 5.0f}, {600.0f, 5.0f}, {800.0f, 1.0f}};
    for (int k = 0; k < 4; k++) {
        printf("%s    {\"ct_primary_a\": %.4g, \"ct_secondary_a\": %.4g, \"a_per_count\": %.9g, "
               "\"ratio\": %.9g, \"secondary_a_at_3000\": %.9g}",
               k ? ",\n" : "", (double)ct[k][0], (double)ct[k][1],
               (double)pu_a_per_count(ct[k][0], 1200.0f), (double)pu_ct_ratio(ct[k][0], ct[k][1]),
               (double)pu_secondary_a(3000.0f, ct[k][0], ct[k][1]));
    }
    printf("\n  ],\n  \"pu_base\": [\n    {\"mva\": 20, \"kv_ll\": 12.47, \"base_a\": %.9g}\n  ],\n",
           (double)pu_base_a_from_mva(20.0f, 12.47f));

    /* Rule vectors: a valid group, then one perturbation per rule. */
    float valid[RULES_IN_COUNT] = {
        600.0f, 0.5f, (float)TOC_IEEE_VI, 0.0f, 120.0f, 1.0f, (float)TOC_IEEE_VI, 0.0f,
        6000.0f, 6000.0f, 80.0f, 120.0f, 0.30f, 200.0f, 240.0f, 0.020f, 0.150f,
        59.3f, 60.5f, 1.5f, 7200.0f,
        1200.0f, 5.0f, 2000.0f, 5.0f, 600.0f, 5.0f, 800.0f, 5.0f,
        20.0f, 0.20f, 25.0f, 60.0f, 2.0f, 5.0f, 3.0f, 4.0f,
    };
    struct { int index; float value; const char *expect; } cases[] = {
        {-1, 0.0f, ""},
        {RULES_IN_51P_TMS, 0.01f, "R-51-002"},
        {RULES_IN_51P_PICKUP_A, 5.0f, "R-51-001"},
        {RULES_IN_51P_CURVE, 42.0f, "R-51-003"},
        {RULES_IN_21_Z1_REACH_PCT, 130.0f, "R-21-001"},
        {RULES_IN_21_Z2_REACH_PCT, 70.0f, "R-21-002"},
        {RULES_IN_50BF_RETRIP_S, 0.200f, "R-BF-002"},
        {RULES_IN_81U_PICKUP_HZ, 61.0f, "R-81-001"},
        {RULES_IN_BAY3_CT_PRIMARY_A, 0.0f, "R-CT-001"},
        {RULES_IN_BAY4_CT_SECONDARY_A, 2.0f, "R-CT-002"},
        {RULES_IN_50P_PICKUP_A, 500.0f, "R-50-001"},
        {RULES_IN_87B_PICKUP_PU, 0.04f, "R-87B-001"},
        {RULES_IN_87B_SLOPE2_PCT, 20.0f, "R-87B-005"},
        {RULES_IN_87B_BAYS_IN_ZONE, 1.0f, "R-87B-008"},
    };
    printf("  \"rules\": [\n");
    for (unsigned k = 0; k < sizeof(cases) / sizeof(cases[0]); k++) {
        float in[RULES_IN_COUNT];
        for (int i = 0; i < RULES_IN_COUNT; i++) {
            in[i] = valid[i];
        }
        if (cases[k].index >= 0) {
            in[cases[k].index] = cases[k].value;
        }
        rule_violation_t v[RULES_MAX_VIOLATIONS];
        int n = settings_rules_check(in, v, RULES_MAX_VIOLATIONS);
        printf("%s    {\"input\": \"%s\", \"value\": %.6g, \"violations\": [",
               k ? ",\n" : "", cases[k].index >= 0 ? settings_rules_input_name(cases[k].index) : "",
               (double)cases[k].value);
        for (int i = 0; i < n; i++) {
            printf("%s\"%s\"", i ? ", " : "", settings_rule_id(v[i].code));
        }
        printf("], \"expect_contains\": \"%s\"}", cases[k].expect);
    }
    printf("\n  ]\n}\n");
    return 0;
}
