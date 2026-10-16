/* Characterization: pins the operated elements, trip outputs and TRIP1 /
 * TRIPB timing of every test-set scenario on the closed loop. Any change to
 * this table is a behaviour change and must be justified in the PR. */
#include "test_harness.h"
#include "ied_loop.h"

typedef struct {
    const char *scenario;
    elem_mask_t target;
    bool trip1;
    bool tripb;
    int32_t trip1_tick;   /* tick of the first TRIP1 event, -1 if none */
    int32_t tripb_tick;   /* tick of the first TRIPB event, -1 if none */
} pin_t;

static int32_t event_tick(uint16_t code)
{
    const soe_event_t *ev = loop_find(code, 0);
    return ev != NULL ? (int32_t)ev->tick : -1;
}

static void pin(const pin_t *p)
{
    TH_CASE(p->scenario);
    const ied_status_t *st = loop_run(testset_find(p->scenario), 0u);
    int32_t t1 = event_tick(SOE_TRIP1_ON);
    int32_t tb = event_tick(SOE_TRIPB_ON);
#ifdef CHAR_PRINT
    printf("    {\"%s\", 0x%04x, %s, %s, %d, %d},\n", p->scenario, st->target,
           st->trip1 ? "true" : "false", st->tripb ? "true" : "false", t1, tb);
#else
    TH_ASSERT(st->target == p->target);
    TH_ASSERT(st->trip1 == p->trip1);
    TH_ASSERT(st->tripb == p->tripb);
    /* +/- one protection pass (4 samples) is the agreed tolerance */
    TH_ASSERT(t1 == p->trip1_tick || (t1 >= 0 && p->trip1_tick >= 0 && abs(t1 - p->trip1_tick) <= 4));
    TH_ASSERT(tb == p->tripb_tick || (tb >= 0 && p->tripb_tick >= 0 && abs(tb - p->tripb_tick) <= 4));
#endif
}

static const pin_t k_pins[] = {
#include "characterization_table.inc"
};

int main(void)
{
    for (size_t k = 0; k < sizeof(k_pins) / sizeof(k_pins[0]); k++) {
        pin(&k_pins[k]);
    }
    return th_report("characterization");
}
