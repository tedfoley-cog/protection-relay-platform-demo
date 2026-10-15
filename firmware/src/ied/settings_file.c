#include "settings_file.h"

#include "toc_curve.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void add_error(settings_result_t *res, int line, const char *key, const char *msg)
{
    if (res == NULL) {
        return;
    }
    if (res->count < SETTINGS_MAX_ERRORS) {
        settings_error_t *e = &res->err[res->count];
        e->line = line;
        snprintf(e->key, sizeof(e->key), "%.31s", key != NULL ? key : "");
        snprintf(e->message, sizeof(e->message), "%.95s", msg);
    }
    res->count++;
}

static bool parse_float(const char *v, float *out)
{
    char *end = NULL;
    float f = strtof(v, &end);
    if (end == v || *end != '\0') {
        return false;
    }
    *out = f;
    return true;
}

typedef enum { K_FLOAT, K_TICKS_S, K_CURVE, K_POLARITY, K_U8, K_CONNECTED } key_kind_t;

typedef struct {
    const char *key;
    key_kind_t kind;
    size_t offset;
} key_def_t;

#define OFF(field) offsetof(ied_settings_t, field)

static const key_def_t k_keys[] = {
    {"VT_V_LN_NOMINAL", K_FLOAT, OFF(v_ln_nominal)},
    {"SOURCE_X_OVER_R", K_FLOAT, OFF(x_over_r)},
    {"50P_PICKUP_A", K_FLOAT, OFF(i50p.pickup_a)},
    {"50P_SECURITY_PASSES", K_U8, OFF(i50p.security_passes)},
    {"50G_PICKUP_A", K_FLOAT, OFF(i50g.pickup_a)},
    {"50G_SECURITY_PASSES", K_U8, OFF(i50g.security_passes)},
    {"51P_PICKUP_A", K_FLOAT, OFF(t51p.pickup_a)},
    {"51P_TMS", K_FLOAT, OFF(t51p.tms)},
    {"51P_CURVE", K_CURVE, OFF(t51p.curve)},
    {"51P_DEFINITE_S", K_FLOAT, OFF(t51p.definite_s)},
    {"51G_PICKUP_A", K_FLOAT, OFF(t51g.pickup_a)},
    {"51G_TMS", K_FLOAT, OFF(t51g.tms)},
    {"51G_CURVE", K_CURVE, OFF(t51g.curve)},
    {"51G_DEFINITE_S", K_FLOAT, OFF(t51g.definite_s)},
    {"21_Z1_LINE_R_OHM", K_FLOAT, OFF(dist.z1_line.re)},
    {"21_Z1_LINE_X_OHM", K_FLOAT, OFF(dist.z1_line.im)},
    {"21_Z1_REACH_PCT", K_FLOAT, OFF(dist.z1_reach_pct)},
    {"21_Z2_REACH_PCT", K_FLOAT, OFF(dist.z2_reach_pct)},
    {"21_Z2_DELAY_S", K_TICKS_S, OFF(dist.z2_delay_ticks)},
    {"21_MIN_LOOP_A", K_FLOAT, OFF(dist.min_loop_a)},
    {"21_MIN_RESIDUAL_A", K_FLOAT, OFF(dist.min_residual_a)},
    {"50BF_PICKUP_A", K_FLOAT, OFF(bf.pickup_a)},
    {"50BF_RETRIP_S", K_FLOAT, OFF(bf.retrip_delay_s)},
    {"50BF_DELAY_S", K_FLOAT, OFF(bf.bf_delay_s)},
    {"81U_PICKUP_HZ", K_FLOAT, OFF(freq.uf_pickup_hz)},
    {"81O_PICKUP_HZ", K_FLOAT, OFF(freq.of_pickup_hz)},
    {"81R_PICKUP_HZ_S", K_FLOAT, OFF(freq.rocof_pickup_hz_s)},
    {"81_UV_INHIBIT_PU", K_FLOAT, OFF(freq.uv_inhibit_pu)},
    {"BAY1_CT_PRIMARY_A", K_FLOAT, OFF(bay[0].ct_primary_a)},
    {"BAY1_CT_SECONDARY_A", K_FLOAT, OFF(bay[0].ct_secondary_a)},
    {"BAY1_POLARITY", K_POLARITY, OFF(bay[0].polarity)},
    {"BAY1_CT_CONNECTED", K_CONNECTED, OFF(bay[0].ct_connected)},
    {"BAY2_CT_PRIMARY_A", K_FLOAT, OFF(bay[1].ct_primary_a)},
    {"BAY2_CT_SECONDARY_A", K_FLOAT, OFF(bay[1].ct_secondary_a)},
    {"BAY2_POLARITY", K_POLARITY, OFF(bay[1].polarity)},
    {"BAY2_CT_CONNECTED", K_CONNECTED, OFF(bay[1].ct_connected)},
    {"BAY3_CT_PRIMARY_A", K_FLOAT, OFF(bay[2].ct_primary_a)},
    {"BAY3_CT_SECONDARY_A", K_FLOAT, OFF(bay[2].ct_secondary_a)},
    {"BAY3_POLARITY", K_POLARITY, OFF(bay[2].polarity)},
    {"BAY3_CT_CONNECTED", K_CONNECTED, OFF(bay[2].ct_connected)},
    {"BAY4_CT_PRIMARY_A", K_FLOAT, OFF(bay[3].ct_primary_a)},
    {"BAY4_CT_SECONDARY_A", K_FLOAT, OFF(bay[3].ct_secondary_a)},
    {"BAY4_POLARITY", K_POLARITY, OFF(bay[3].polarity)},
    {"BAY4_CT_CONNECTED", K_CONNECTED, OFF(bay[3].ct_connected)},
};
#define NUM_KEYS (sizeof(k_keys) / sizeof(k_keys[0]))

bool settings_file_assign(const char *key, const char *value, ied_settings_t *s,
                          settings_result_t *res, int line)
{
    for (size_t k = 0; k < NUM_KEYS; k++) {
        if (strcmp(k_keys[k].key, key) != 0) {
            continue;
        }
        void *field = (uint8_t *)s + k_keys[k].offset;
        float f = 0.0f;
        switch (k_keys[k].kind) {
        case K_FLOAT:
            if (!parse_float(value, &f)) {
                add_error(res, line, key, "value is not a number");
                return false;
            }
            *(float *)field = f;
            return true;
        case K_TICKS_S:
            if (!parse_float(value, &f) || f < 0.0f) {
                add_error(res, line, key, "value is not a non-negative time in seconds");
                return false;
            }
            *(uint32_t *)field = (uint32_t)(f * (float)IED_SAMPLE_RATE_HZ + 0.5f);
            return true;
        case K_U8:
            if (!parse_float(value, &f) || f < 1.0f || f > 255.0f || f != (float)(int)f) {
                add_error(res, line, key, "value must be an integer 1..255");
                return false;
            }
            *(uint8_t *)field = (uint8_t)f;
            return true;
        case K_CURVE: {
            toc_curve_t c = toc_curve_from_name(value);
            if (c == TOC_CURVE_COUNT) {
                add_error(res, line, key, settings_rule_message(RULE_51_CURVE_VALID));
                return false;
            }
            *(toc_curve_t *)field = c;
            return true;
        }
        case K_POLARITY:
            if (strcmp(value, "BUS") == 0) {
                *(bay_polarity_t *)field = BAY_POLARITY_TOWARD_BUS;
            } else if (strcmp(value, "LINE") == 0) {
                *(bay_polarity_t *)field = BAY_POLARITY_TOWARD_LINE;
            } else {
                add_error(res, line, key, "polarity must be BUS or LINE");
                return false;
            }
            return true;
        case K_CONNECTED:
            if (strcmp(value, "YES") == 0) {
                *(uint8_t *)field = 1u;
            } else if (strcmp(value, "NO") == 0) {
                *(uint8_t *)field = 0u;
            } else {
                add_error(res, line, key, "CT_CONNECTED must be YES or NO");
                return false;
            }
            return true;
        }
    }
    add_error(res, line, key, "unknown setting");
    return false;
}

void settings_rules_input(const ied_settings_t *s, float out[RULES_IN_COUNT])
{
    out[RULES_IN_51P_PICKUP_A] = s->t51p.pickup_a;
    out[RULES_IN_51P_TMS] = s->t51p.tms;
    out[RULES_IN_51P_CURVE] = (float)s->t51p.curve;
    out[RULES_IN_51P_DEFINITE_S] = s->t51p.definite_s;
    out[RULES_IN_51G_PICKUP_A] = s->t51g.pickup_a;
    out[RULES_IN_51G_TMS] = s->t51g.tms;
    out[RULES_IN_51G_CURVE] = (float)s->t51g.curve;
    out[RULES_IN_51G_DEFINITE_S] = s->t51g.definite_s;
    out[RULES_IN_50P_PICKUP_A] = s->i50p.pickup_a;
    out[RULES_IN_50G_PICKUP_A] = s->i50g.pickup_a;
    out[RULES_IN_21_Z1_REACH_PCT] = s->dist.z1_reach_pct;
    out[RULES_IN_21_Z2_REACH_PCT] = s->dist.z2_reach_pct;
    out[RULES_IN_21_Z2_DELAY_S] = (float)s->dist.z2_delay_ticks / (float)IED_SAMPLE_RATE_HZ;
    out[RULES_IN_21_MIN_LOOP_A] = s->dist.min_loop_a;
    out[RULES_IN_50BF_PICKUP_A] = s->bf.pickup_a;
    out[RULES_IN_50BF_RETRIP_S] = s->bf.retrip_delay_s;
    out[RULES_IN_50BF_DELAY_S] = s->bf.bf_delay_s;
    out[RULES_IN_81U_PICKUP_HZ] = s->freq.uf_pickup_hz;
    out[RULES_IN_81O_PICKUP_HZ] = s->freq.of_pickup_hz;
    out[RULES_IN_81R_PICKUP_HZ_S] = s->freq.rocof_pickup_hz_s;
    out[RULES_IN_VT_V_LN_NOMINAL] = s->v_ln_nominal;
    for (int bay = 0; bay < 4; bay++) {
        out[RULES_IN_BAY1_CT_PRIMARY_A + 2 * bay] = s->bay[bay].ct_primary_a;
        out[RULES_IN_BAY1_CT_SECONDARY_A + 2 * bay] = s->bay[bay].ct_secondary_a;
    }
}

int settings_file_check(const ied_settings_t *s, settings_result_t *res)
{
    float in[RULES_IN_COUNT];
    rule_violation_t v[RULES_MAX_VIOLATIONS];
    settings_rules_input(s, in);
    int n = settings_rules_check(in, v, RULES_MAX_VIOLATIONS);
    for (int k = 0; k < n && k < RULES_MAX_VIOLATIONS; k++) {
        char msg[96];
        snprintf(msg, sizeof(msg), "%s", settings_rule_message(v[k].code));
        char key[32];
        if (v[k].code == RULE_CT_PRIMARY_RANGE || v[k].code == RULE_CT_SECONDARY_VALUE) {
            snprintf(key, sizeof(key), "BAY%d", v[k].detail + 1);
        } else {
            snprintf(key, sizeof(key), "%s", settings_rule_id(v[k].code));
        }
        add_error(res, 0, key, msg);
    }
    return n;
}

static void trim(char *s)
{
    char *p = s;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    memmove(s, p, strlen(p) + 1);
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r')) {
        s[--n] = '\0';
    }
}

bool settings_file_parse(const char *text, const ied_settings_t *base, ied_settings_t *out,
                         settings_result_t *res)
{
    if (res != NULL) {
        memset(res, 0, sizeof(*res));
    }
    if (text == NULL || base == NULL || out == NULL) {
        add_error(res, 0, "", "null argument");
        return false;
    }
    ied_settings_t work = *base;
    int line = 0;
    bool ok = true;
    const char *p = text;
    while (*p != '\0') {
        char buf[160];
        size_t n = 0;
        while (p[n] != '\0' && p[n] != '\n') {
            n++;
        }
        line++;
        if (n >= sizeof(buf)) {
            add_error(res, line, "", "line too long");
            ok = false;
        } else {
            memcpy(buf, p, n);
            buf[n] = '\0';
            char *hash = strchr(buf, '#');
            if (hash != NULL) {
                *hash = '\0';
            }
            trim(buf);
            if (buf[0] != '\0') {
                char *eq = strchr(buf, '=');
                if (eq == NULL) {
                    add_error(res, line, buf, "expected KEY = VALUE");
                    ok = false;
                } else {
                    *eq = '\0';
                    trim(buf);
                    trim(eq + 1);
                    if (!settings_file_assign(buf, eq + 1, &work, res, line)) {
                        ok = false;
                    }
                }
            }
        }
        p += n;
        if (*p == '\n') {
            p++;
        }
    }
    if (settings_file_check(&work, res) > 0) {
        ok = false;
    }
    if (ok) {
        ied_settings_finalize(&work);
        *out = work;
    }
    return ok;
}

size_t settings_file_write(const ied_settings_t *s, char *buf, size_t len)
{
    size_t n = 0;
    n += (size_t)snprintf(buf + n, len > n ? len - n : 0, "# Relay settings group (KEY = VALUE)\n");
    for (size_t k = 0; k < NUM_KEYS && n < len; k++) {
        const void *field = (const uint8_t *)s + k_keys[k].offset;
        char val[32];
        switch (k_keys[k].kind) {
        case K_FLOAT:
            snprintf(val, sizeof(val), "%g", (double)*(const float *)field);
            break;
        case K_TICKS_S:
            snprintf(val, sizeof(val), "%g",
                     (double)*(const uint32_t *)field / (double)IED_SAMPLE_RATE_HZ);
            break;
        case K_U8:
            snprintf(val, sizeof(val), "%u", *(const uint8_t *)field);
            break;
        case K_CURVE:
            snprintf(val, sizeof(val), "%s", toc_curve_name(*(const toc_curve_t *)field));
            break;
        case K_POLARITY:
            snprintf(val, sizeof(val), "%s",
                     *(const bay_polarity_t *)field == BAY_POLARITY_TOWARD_BUS ? "BUS" : "LINE");
            break;
        case K_CONNECTED:
            snprintf(val, sizeof(val), "%s", *(const uint8_t *)field ? "YES" : "NO");
            break;
        }
        n += (size_t)snprintf(buf + n, len > n ? len - n : 0, "%s = %s\n", k_keys[k].key, val);
    }
    return n < len ? n : len - 1;
}
