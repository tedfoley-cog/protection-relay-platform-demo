/* Relay data model: operands, actual values and event records.
 *
 * This header holds TYPES ONLY. No settings, no scaling constants, no logic:
 * tools/check_failure_modes.py fails the build if any of those leak in here.
 * Every protection element exposes the same operand shape (per-phase and
 * overall pickup / operate / drop-out) so the trip matrix, the SOE recorder,
 * the DNP3 point map and the IEC 61850 mapping (docs/platform/DATA_MODEL.md)
 * read elements uniformly.
 */
#ifndef IED_OPERANDS_H
#define IED_OPERANDS_H

#include <stdbool.h>
#include <stdint.h>

/* One element's operands. Phase slots are A, B, C; `any` is the overall. */
typedef struct {
    bool pickup[3];
    bool operate[3];
    bool dropout[3];      /* true for the pass in which the phase reset */
    bool any_pickup;
    bool any_operate;
    bool any_dropout;
    bool blocked;         /* held by invalid input or an external block */
    bool enabled;
} elem_operands_t;

/* Measured quantities an element publishes as actual values (primary units). */
typedef struct {
    float phase[3];       /* per-phase operating quantity */
    float overall;        /* element-defined (max, residual, differential...) */
    float timer_s;        /* elapsed timer or accumulated fraction */
} elem_actual_values_t;

typedef struct {
    uint32_t soc;
    uint32_t usec;
    uint16_t element;     /* elem_id_t */
    uint8_t phase_mask;   /* bit 0 A, 1 B, 2 C */
    uint8_t kind;         /* 0 pickup, 1 operate, 2 dropout, 3 blocked */
    float value;
} elem_event_t;

#endif /* IED_OPERANDS_H */
