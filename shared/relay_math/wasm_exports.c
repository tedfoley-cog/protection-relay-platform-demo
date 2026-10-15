/* Flat C ABI for the setting tool's WASM module (built by `make wasm`).
 * Every function here is a thin wrapper; no math lives in this file. */
#include "pu_scale.h"
#include "settings_rules.h"
#include "toc_curve.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define RM_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define RM_EXPORT
#endif

static rule_violation_t s_violations[RULES_MAX_VIOLATIONS];

RM_EXPORT int rm_curve_count(void) { return (int)TOC_CURVE_COUNT; }
RM_EXPORT const char *rm_curve_name(int curve) { return toc_curve_name((toc_curve_t)curve); }
RM_EXPORT int rm_curve_from_name(const char *name) { return (int)toc_curve_from_name(name); }

RM_EXPORT float rm_toc_operate_time(int curve, float tms, float definite_s, float multiple)
{
    return toc_curve_operate_time((toc_curve_t)curve, tms, definite_s, multiple);
}

RM_EXPORT float rm_toc_reset_time(int curve, float tms, float multiple)
{
    return toc_curve_reset_time((toc_curve_t)curve, tms, multiple);
}

RM_EXPORT float rm_a_per_count(float ct_primary_a, float counts_per_rated)
{
    return pu_a_per_count(ct_primary_a, counts_per_rated);
}
RM_EXPORT float rm_ct_ratio(float p, float s) { return pu_ct_ratio(p, s); }
RM_EXPORT float rm_secondary_a(float a, float p, float s) { return pu_secondary_a(a, p, s); }
RM_EXPORT float rm_base_a_from_mva(float mva, float kv) { return pu_base_a_from_mva(mva, kv); }
RM_EXPORT float rm_pu_from_a(float a, float base) { return pu_from_a(a, base); }

RM_EXPORT int rm_rules_input_count(void) { return (int)RULES_IN_COUNT; }
RM_EXPORT const char *rm_rules_input_name(int i) { return settings_rules_input_name(i); }
RM_EXPORT int rm_rule_count(void) { return (int)RULE_COUNT; }
RM_EXPORT const char *rm_rule_id(int code) { return settings_rule_id(code); }
RM_EXPORT const char *rm_rule_message(int code) { return settings_rule_message(code); }

/* Runs the rules on `in` (RULES_IN_COUNT floats) and returns the violation
 * count; rm_violation_* read the k-th result. */
RM_EXPORT int rm_rules_check(const float *in)
{
    int n = settings_rules_check(in, s_violations, RULES_MAX_VIOLATIONS);
    return n > RULES_MAX_VIOLATIONS ? RULES_MAX_VIOLATIONS : n;
}
RM_EXPORT int rm_violation_code(int k) { return s_violations[k].code; }
RM_EXPORT int rm_violation_detail(int k) { return s_violations[k].detail; }
RM_EXPORT float rm_violation_value(int k) { return s_violations[k].value; }
