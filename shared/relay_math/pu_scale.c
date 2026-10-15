#include "pu_scale.h"

#define PU_SQRT3 1.7320508075688772f

float pu_a_per_count(float ct_primary_a, float counts_per_rated)
{
    if (ct_primary_a <= 0.0f || counts_per_rated <= 0.0f) {
        return 0.0f;
    }
    return ct_primary_a / counts_per_rated;
}

float pu_ct_ratio(float ct_primary_a, float ct_secondary_a)
{
    if (ct_primary_a <= 0.0f || ct_secondary_a <= 0.0f) {
        return 0.0f;
    }
    return ct_primary_a / ct_secondary_a;
}

float pu_secondary_a(float primary_a, float ct_primary_a, float ct_secondary_a)
{
    float ratio = pu_ct_ratio(ct_primary_a, ct_secondary_a);
    return ratio > 0.0f ? primary_a / ratio : 0.0f;
}

float pu_base_a_from_mva(float mva, float kv_ll)
{
    if (mva <= 0.0f || kv_ll <= 0.0f) {
        return 0.0f;
    }
    return mva * 1000.0f / (PU_SQRT3 * kv_ll);
}

float pu_from_a(float value_a, float base_a)
{
    return base_a > 0.0f ? value_a / base_a : 0.0f;
}
