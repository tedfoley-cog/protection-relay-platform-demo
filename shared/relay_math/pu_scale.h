/* Measurement-chain scaling helpers shared by firmware and the setting tool.
 *
 * Primary amps are the platform's working unit: every bay is brought to
 * primary amps from its own CT ratio first, and per-unit conversion is an
 * explicit second step against a base the element designer chooses.
 */
#ifndef RELAY_MATH_PU_SCALE_H
#define RELAY_MATH_PU_SCALE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Primary amps per ADC count for a current input whose CT rated primary maps
 * to `counts_per_rated` counts. Returns 0 for a non-positive ratio. */
float pu_a_per_count(float ct_primary_a, float counts_per_rated);

/* CT ratio as a dimensionless number (e.g. 1200:5 -> 240). 0 when invalid. */
float pu_ct_ratio(float ct_primary_a, float ct_secondary_a);

/* Secondary amps the CT delivers for `primary_a`. */
float pu_secondary_a(float primary_a, float ct_primary_a, float ct_secondary_a);

/* Base current for a three-phase base of `mva` at line-to-line `kv_ll`. */
float pu_base_a_from_mva(float mva, float kv_ll);

/* Per-unit value of `value_a` on `base_a`; 0 when base is non-positive. */
float pu_from_a(float value_a, float base_a);

#ifdef __cplusplus
}
#endif
#endif /* RELAY_MATH_PU_SCALE_H */
