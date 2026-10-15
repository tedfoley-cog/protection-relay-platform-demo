#include "toc_curve.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

static const char *const k_names[TOC_CURVE_COUNT] = {
    "IEC_SI", "IEC_VI", "IEC_EI", "IEC_LTI", "IEEE_MI", "IEEE_VI", "IEEE_EI", "DEFINITE",
};

toc_curve_def_t toc_curve_def(toc_curve_t curve)
{
    switch (curve) {
    case TOC_IEC_VI:
        return (toc_curve_def_t){13.5f, 0.0f, 1.0f, 0.0f};
    case TOC_IEC_EI:
        return (toc_curve_def_t){80.0f, 0.0f, 2.0f, 0.0f};
    case TOC_IEC_LTI:
        return (toc_curve_def_t){120.0f, 0.0f, 1.0f, 0.0f};
    case TOC_IEEE_MI:
        return (toc_curve_def_t){0.0515f, 0.1140f, 0.02f, 4.85f};
    case TOC_IEEE_VI:
        return (toc_curve_def_t){19.61f, 0.491f, 2.0f, 21.6f};
    case TOC_IEEE_EI:
        return (toc_curve_def_t){28.2f, 0.1217f, 2.0f, 29.1f};
    case TOC_IEC_SI:
    case TOC_DEFINITE:
    default:
        return (toc_curve_def_t){0.14f, 0.0f, 0.02f, 0.0f};
    }
}

const char *toc_curve_name(toc_curve_t curve)
{
    return (curve >= 0 && curve < TOC_CURVE_COUNT) ? k_names[curve] : "UNKNOWN";
}

toc_curve_t toc_curve_from_name(const char *name)
{
    if (name == NULL) {
        return TOC_CURVE_COUNT;
    }
    for (int k = 0; k < (int)TOC_CURVE_COUNT; k++) {
        if (strcmp(k_names[k], name) == 0) {
            return (toc_curve_t)k;
        }
    }
    return TOC_CURVE_COUNT;
}

float toc_curve_operate_time(toc_curve_t curve, float tms, float definite_s, float multiple)
{
    if (multiple <= 1.0f || curve < 0 || curve >= TOC_CURVE_COUNT) {
        return -1.0f;
    }
    if (curve == TOC_DEFINITE) {
        return definite_s;
    }
    float m = multiple > TOC_MAX_MULTIPLE ? TOC_MAX_MULTIPLE : multiple;
    toc_curve_def_t c = toc_curve_def(curve);
    float denom = powf(m, c.p) - 1.0f;
    if (denom <= 1e-6f) {
        return -1.0f;
    }
    return tms * (c.a / denom + c.b);
}

float toc_curve_reset_time(toc_curve_t curve, float tms, float multiple)
{
    if (curve == TOC_DEFINITE || curve < 0 || curve >= TOC_CURVE_COUNT) {
        return 0.0f;
    }
    toc_curve_def_t c = toc_curve_def(curve);
    if (c.tr <= 0.0f || multiple >= 1.0f) {
        return 0.0f;
    }
    return tms * c.tr / (1.0f - multiple * multiple);
}
