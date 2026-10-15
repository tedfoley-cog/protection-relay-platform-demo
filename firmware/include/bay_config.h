/* Bay table for the measurement chain.
 *
 * Each bay is one connection to the protected bus with its own three-phase CT
 * set. The relay scales every bay to primary amps from its CT ratio, so
 * elements compare bays in primary units; per-unit scaling is a separate,
 * explicit step (shared/relay_math/pu_scale.h) because the per-unit base of
 * a multi-bay zone is a design decision, not a platform constant.
 *
 * Polarity: BAY_POLARITY_TOWARD_BUS means current flowing into the bus is
 * positive. BAY_POLARITY_TOWARD_LINE means current flowing out of the bus
 * into the bay is positive (the convention of a feeder relay, kept for bay 1
 * so the existing directional elements are unchanged).
 */
#ifndef BAY_CONFIG_H
#define BAY_CONFIG_H

#include "ied_config.h"

#include <stdint.h>

typedef enum {
    BAY_POLARITY_TOWARD_BUS = 1,
    BAY_POLARITY_TOWARD_LINE = -1
} bay_polarity_t;

typedef enum {
    BAY_ROLE_FEEDER = 0,
    BAY_ROLE_INFEED,      /* transformer / source connection */
    BAY_ROLE_TIE,
    BAY_ROLE_SPARE        /* no CT connected: channels are not valid inputs */
} bay_role_t;

typedef struct {
    const char *name;
    bay_role_t role;
    float ct_primary_a;      /* CT rated primary, A */
    float ct_secondary_a;    /* CT rated secondary, A (1 or 5) */
    bay_polarity_t polarity;
    uint8_t ct_connected;    /* 0: no CT source, inputs flagged invalid */
} bay_config_t;

#endif /* BAY_CONFIG_H */
