/* Element configuration rules — one implementation, run by the relay when a
 * setting group is written (protocol or file) and by the setting tool as the
 * user types. The input is a flat float vector so the same layout crosses the
 * C/WASM boundary without marshalling; RULES_IN_* indices are the contract.
 *
 * Ranges are the platform's existing element ranges (docs/elements/51,
 * docs/platform/MEASUREMENT_CHAIN.md). A rule that fails rejects the whole
 * write: the relay never applies a partially valid group.
 */
#ifndef RELAY_MATH_SETTINGS_RULES_H
#define RELAY_MATH_SETTINGS_RULES_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RULES_IN_51P_PICKUP_A = 0,
    RULES_IN_51P_TMS,
    RULES_IN_51P_CURVE,          /* toc_curve_t as float */
    RULES_IN_51P_DEFINITE_S,
    RULES_IN_51G_PICKUP_A,
    RULES_IN_51G_TMS,
    RULES_IN_51G_CURVE,
    RULES_IN_51G_DEFINITE_S,
    RULES_IN_50P_PICKUP_A,
    RULES_IN_50G_PICKUP_A,
    RULES_IN_21_Z1_REACH_PCT,
    RULES_IN_21_Z2_REACH_PCT,
    RULES_IN_21_Z2_DELAY_S,
    RULES_IN_21_MIN_LOOP_A,
    RULES_IN_50BF_PICKUP_A,
    RULES_IN_50BF_RETRIP_S,
    RULES_IN_50BF_DELAY_S,
    RULES_IN_81U_PICKUP_HZ,
    RULES_IN_81O_PICKUP_HZ,
    RULES_IN_81R_PICKUP_HZ_S,
    RULES_IN_VT_V_LN_NOMINAL,
    RULES_IN_BAY1_CT_PRIMARY_A,
    RULES_IN_BAY1_CT_SECONDARY_A,
    RULES_IN_BAY2_CT_PRIMARY_A,
    RULES_IN_BAY2_CT_SECONDARY_A,
    RULES_IN_BAY3_CT_PRIMARY_A,
    RULES_IN_BAY3_CT_SECONDARY_A,
    RULES_IN_BAY4_CT_PRIMARY_A,
    RULES_IN_BAY4_CT_SECONDARY_A,
    RULES_IN_87B_BASE_MVA,
    RULES_IN_87B_PICKUP_PU,
    RULES_IN_87B_SLOPE1_PCT,
    RULES_IN_87B_SLOPE2_PCT,
    RULES_IN_87B_BREAKPOINT_PU,
    RULES_IN_87B_CTS_ALARM_PCT,
    RULES_IN_87B_CTS_DELAY_S,
    RULES_IN_87B_BAYS_IN_ZONE,   /* number of bays set in zone, as float */
    RULES_IN_COUNT
} rules_input_index_t;

/* Rule identifiers; stable, quoted in the manual and the tool. */
typedef enum {
    RULE_51_PICKUP_RANGE = 0,  /* R-51-001 */
    RULE_51_TMS_RANGE,         /* R-51-002 */
    RULE_51_CURVE_VALID,       /* R-51-003 */
    RULE_51_DEFINITE_RANGE,    /* R-51-004 */
    RULE_51_PICKUP_VS_CT,      /* R-51-005 pickup within 0.1..20 x bay-1 CT primary */
    RULE_50_ABOVE_51,          /* R-50-001 */
    RULE_21_Z1_RANGE,          /* R-21-001 */
    RULE_21_Z2_RANGE,          /* R-21-002 */
    RULE_21_ZONE_ORDER,        /* R-21-003 Z1 reach < Z2 reach */
    RULE_21_Z2_DELAY_RANGE,    /* R-21-004 */
    RULE_21_MIN_LOOP_RANGE,    /* R-21-005 */
    RULE_50BF_PICKUP_RANGE,    /* R-BF-001 */
    RULE_50BF_TIMER_ORDER,     /* R-BF-002 retrip < bf delay */
    RULE_81_ORDER,             /* R-81-001 81U < nominal < 81O */
    RULE_81_ROCOF_RANGE,       /* R-81-002 */
    RULE_VT_NOMINAL_RANGE,     /* R-VT-001 */
    RULE_CT_PRIMARY_RANGE,     /* R-CT-001 (detail = bay index) */
    RULE_CT_SECONDARY_VALUE,   /* R-CT-002 (detail = bay index) */
    RULE_87B_PICKUP_RANGE,     /* R-87B-001 */
    RULE_87B_SLOPE1_RANGE,     /* R-87B-002 */
    RULE_87B_SLOPE2_RANGE,     /* R-87B-003 */
    RULE_87B_BREAKPOINT_RANGE, /* R-87B-004 */
    RULE_87B_SLOPE_ORDER,      /* R-87B-005 slope 2 >= slope 1 */
    RULE_87B_CTS_PCT_RANGE,    /* R-87B-006 */
    RULE_87B_CTS_DELAY_RANGE,  /* R-87B-007 */
    RULE_87B_MIN_BAYS,         /* R-87B-008 at least two bays in zone */
    RULE_87B_BASE_RANGE,       /* R-87B-009 */
    RULE_COUNT
} rule_code_t;

typedef struct {
    int code;      /* rule_code_t */
    int detail;    /* bay index for CT rules, otherwise 0 */
    float value;   /* offending value */
} rule_violation_t;

#define RULES_MAX_VIOLATIONS 32

/* Returns the number of violations written (<= max). */
int settings_rules_check(const float in[RULES_IN_COUNT], rule_violation_t *out, int max);

const char *settings_rule_id(int code);        /* "R-51-002" */
const char *settings_rule_message(int code);   /* "51 TMS must be 0.05..15.0" */
const char *settings_rules_input_name(int index);  /* "51P_TMS" */

#ifdef __cplusplus
}
#endif
#endif /* RELAY_MATH_SETTINGS_RULES_H */
