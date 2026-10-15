/* Protocol-write path: applies settings to the relay WITHOUT the setting tool,
 * exactly as a settings write over the engineering protocol would — the
 * firmware's own parser and the shared rules decide, so the firmware-side
 * validation is testable from the command line and from CI.
 *
 *   settings_write <file.set> [KEY=VALUE ...] [--out <file.set>]
 *
 * Reads <file.set> over the relay defaults, applies the overrides in order,
 * and prints ACCEPTED (exit 0) or REJECTED with every error (exit 2). With
 * --out the accepted group is written back in the file format. */
#include "settings_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1u);
    if (buf == NULL || fread(buf, 1, (size_t)n, f) != (size_t)n) {
        fclose(f);
        free(buf);
        return NULL;
    }
    buf[n] = '\0';
    fclose(f);
    return buf;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: settings_write <file.set> [KEY=VALUE ...] [--out <file.set>]\n");
        return 1;
    }
    char *text = read_file(argv[1]);
    if (text == NULL) {
        perror(argv[1]);
        return 1;
    }
    ied_settings_t group;
    settings_result_t res;
    bool ok = settings_file_parse(text, ied_default_settings(), &group, &res);
    free(text);
    if (!ok) {
        printf("REJECTED %s: %d error(s)\n", argv[1], res.count);
        for (int k = 0; k < res.count && k < SETTINGS_MAX_ERRORS; k++) {
            printf("  line %d %s: %s\n", res.err[k].line, res.err[k].key, res.err[k].message);
        }
        return 2;
    }

    const char *out_path = NULL;
    memset(&res, 0, sizeof(res));
    int applied = 0;
    for (int a = 2; a < argc; a++) {
        if (strcmp(argv[a], "--out") == 0 && a + 1 < argc) {
            out_path = argv[++a];
            continue;
        }
        char *eq = strchr(argv[a], '=');
        if (eq == NULL) {
            fprintf(stderr, "not KEY=VALUE: %s\n", argv[a]);
            return 1;
        }
        *eq = '\0';
        if (settings_file_assign(argv[a], eq + 1, &group, &res, 0)) {
            applied++;
        }
    }
    settings_file_check(&group, &res);
    if (res.count > 0) {
        printf("REJECTED write of %d setting(s): %d error(s); active group unchanged\n", applied,
               res.count);
        for (int k = 0; k < res.count && k < SETTINGS_MAX_ERRORS; k++) {
            printf("  %s: %s\n", res.err[k].key, res.err[k].message);
        }
        return 2;
    }
    ied_settings_finalize(&group);
    printf("ACCEPTED %d override(s); 51P %s pickup %g A TMS %g; 21 Z1 %g%% Z2 %g%%; bay CTs %g/%g/%g/%g A\n",
           applied, toc_curve_name(group.t51p.curve), (double)group.t51p.pickup_a,
           (double)group.t51p.tms, (double)group.dist.z1_reach_pct, (double)group.dist.z2_reach_pct,
           (double)group.bay[0].ct_primary_a, (double)group.bay[1].ct_primary_a,
           (double)group.bay[2].ct_primary_a, (double)group.bay[3].ct_primary_a);
    if (out_path != NULL) {
        static char buf[8192];
        size_t n = settings_file_write(&group, buf, sizeof(buf));
        FILE *f = fopen(out_path, "wb");
        if (f == NULL) {
            perror(out_path);
            return 1;
        }
        fwrite(buf, 1, n, f);
        fclose(f);
        printf("WROTE %s\n", out_path);
    }
    return 0;
}
