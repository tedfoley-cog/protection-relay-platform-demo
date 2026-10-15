/* Inverse-time overcurrent characteristics — the one implementation shared by
 * the relay firmware and the setting tool (compiled to WASM).
 *
 * IEC 60255-151:2009 eq. (1):  t(M) = TMS * k / (M^alpha - 1)
 * IEEE C37.112-2018 eq. (1):   t(M) = TD * (A / (M^p - 1) + B)
 * IEEE reset (eq. (2)):        t_r(M) = TD * tr / (1 - M^2),  M < 1
 * Constants are the published values; none are tuned.
 */
#ifndef RELAY_MATH_TOC_CURVE_H
#define RELAY_MATH_TOC_CURVE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TOC_IEC_SI = 0,   /* IEC standard inverse   k=0.14  alpha=0.02 */
    TOC_IEC_VI,       /* IEC very inverse       k=13.5  alpha=1    */
    TOC_IEC_EI,       /* IEC extremely inverse  k=80    alpha=2    */
    TOC_IEC_LTI,      /* IEC long-time inverse  k=120   alpha=1    */
    TOC_IEEE_MI,      /* IEEE moderately inverse A=0.0515 B=0.1140 p=0.02 tr=4.85 */
    TOC_IEEE_VI,      /* IEEE very inverse       A=19.61  B=0.491  p=2    tr=21.6 */
    TOC_IEEE_EI,      /* IEEE extremely inverse  A=28.2   B=0.1217 p=2    tr=29.1 */
    TOC_DEFINITE,
    TOC_CURVE_COUNT
} toc_curve_t;

typedef struct {
    float a;      /* IEC k, IEEE A */
    float b;      /* IEEE B (0 for IEC) */
    float p;      /* exponent */
    float tr;     /* IEEE reset constant (0: instantaneous reset) */
} toc_curve_def_t;

#define TOC_DROPOUT_RATIO 0.95f
#define TOC_MAX_MULTIPLE  20.0f  /* curve evaluated up to 20 x Is */

toc_curve_def_t toc_curve_def(toc_curve_t curve);
const char *toc_curve_name(toc_curve_t curve);
/* Parses "IEC_SI", "IEEE_VI", "DEFINITE", ...; returns TOC_CURVE_COUNT when unknown. */
toc_curve_t toc_curve_from_name(const char *name);

/* Operate time for a constant multiple M = I / Is; negative when M <= 1. */
float toc_curve_operate_time(toc_curve_t curve, float tms, float definite_s, float multiple);
/* Disc reset time for M < 1 (IEEE curves); 0 for instantaneous-reset curves. */
float toc_curve_reset_time(toc_curve_t curve, float tms, float multiple);

#ifdef __cplusplus
}
#endif
#endif /* RELAY_MATH_TOC_CURVE_H */
