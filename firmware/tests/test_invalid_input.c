/* Platform rule: missing or invalid input is never read as zero and never
 * produces a spurious operation. Elements fed by an invalid channel hold. */
#include "test_harness.h"
#include "ied_loop.h"

static void test_lost_ct_blocks_elements(void)
{
    TH_CASE("lost bay-1 CT under load: elements held, inputs flagged, no trip");
    const ied_status_t *st = loop_run(testset_find("ct_lost_bay1_load"), 0u);
    TH_ASSERT(!st->trip1);
    TH_ASSERT(!st->tripb);
    TH_ASSERT(st->operated == 0u);
    TH_ASSERT(st->invalid_mask == (IED_BAY_MASK(0) | IED_CH_BIT(IED_CH_IN)));
    TH_ASSERT(st->blocked_passes > 0u);
}

static void test_disconnected_bay_is_invalid(void)
{
    TH_CASE("a bay with no CT source is flagged invalid from power-up");
    ied_settings_t s = *ied_default_settings();
    s.bay[3].ct_connected = 0u;
    ied_hooks_t hooks = {NULL, NULL, NULL};
    ied_init(&s, 1790000000u, &hooks);
    int16_t raw[IED_NUM_CHANNELS] = {0};
    for (int k = 0; k < 64; k++) {
        (void)ied_acq_isr(raw);
        (void)ied_protection_task();
    }
    TH_ASSERT((ied_status()->invalid_mask & IED_BAY_MASK(3)) == IED_BAY_MASK(3));
    TH_ASSERT((ied_status()->invalid_mask & IED_BAY_MASK(0)) == 0u);
}

static void test_valid_load_not_blocked(void)
{
    TH_CASE("healthy load: nothing blocked");
    const ied_status_t *st = loop_run(testset_find("steady_load"), 0u);
    TH_ASSERT(st->invalid_mask == 0u);
    TH_ASSERT(st->blocked_passes == 0u);
}

int main(void)
{
    test_lost_ct_blocks_elements();
    test_disconnected_bay_is_invalid();
    test_valid_load_not_blocked();
    return th_report("invalid_input");
}
