/* Relay settings file: flat `KEY = VALUE` text, one setting per line, `#`
 * comments. Written by the setting tool and by the protocol-write path
 * (sim/settings_write.c); read back by the relay at power-up.
 *
 * A write is all-or-nothing: unknown keys, unparsable values and any
 * violation of the shared rules (shared/relay_math/settings_rules.h) reject
 * the whole file and the active group is untouched.
 */
#ifndef SETTINGS_FILE_H
#define SETTINGS_FILE_H

#include "ied_app.h"
#include "settings_rules.h"

#include <stddef.h>

#define SETTINGS_MAX_ERRORS 16

typedef struct {
    int line;                 /* 0 for rule violations */
    char key[32];
    char message[96];
} settings_error_t;

typedef struct {
    settings_error_t err[SETTINGS_MAX_ERRORS];
    int count;                /* total errors, may exceed SETTINGS_MAX_ERRORS */
} settings_result_t;

/* Parses `text` over a copy of `base` into `out`, then runs the shared rules.
 * Returns true when the group is accepted. */
bool settings_file_parse(const char *text, const ied_settings_t *base, ied_settings_t *out,
                         settings_result_t *res);

/* Applies a single `KEY=VALUE` assignment (used by the protocol write path). */
bool settings_file_assign(const char *key, const char *value, ied_settings_t *s,
                          settings_result_t *res, int line);

/* Fills the shared rules input vector from a settings group. */
void settings_rules_input(const ied_settings_t *s, float out[RULES_IN_COUNT]);

/* Runs the shared rules; appends violations to `res`. Returns violation count. */
int settings_file_check(const ied_settings_t *s, settings_result_t *res);

/* Serialises the group in the file format. Returns bytes written (excluding NUL). */
size_t settings_file_write(const ied_settings_t *s, char *buf, size_t len);

#endif /* SETTINGS_FILE_H */
