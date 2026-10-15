/* Compile-time configuration of the relay platform's real-time core.
 *
 * This header is the single source of the platform figures quoted in
 * docs/platform/PLATFORM_FIGURES.md: sample rate, protection execution rate,
 * phasor window, nominal frequency, channel map and task budgets. Change a
 * figure here and the document, the test set and the Renode timing report
 * all follow; do not restate the numbers anywhere else.
 *
 * Sampling is fixed-rate and disciplined to the station PPS: 32 samples per
 * nominal 60 Hz cycle, 1920 samples per UTC second. Every rate below is an
 * integer divisor of the sample rate so the cyclic schedule repeats exactly
 * once per second and no task ever sees a fractional period. Phasors are
 * full-cycle DFTs (32 samples, rectangular window) refreshed every protection
 * pass; see src/dsp/phasor.h for the filter response.
 */
#ifndef IED_CONFIG_H
#define IED_CONFIG_H

#include <stdint.h>

#define IED_F_NOMINAL_HZ        60.0f
#define IED_SAMPLES_PER_CYCLE   32u
#define IED_SAMPLE_RATE_HZ      1920u
#define IED_SAMPLE_PERIOD_S     (1.0f / (float)IED_SAMPLE_RATE_HZ)

/* Protection pass every 4 samples: 8 passes per cycle, 2.083 ms period. */
#define IED_PROT_DECIMATION     4u
#define IED_PROT_RATE_HZ        (IED_SAMPLE_RATE_HZ / IED_PROT_DECIMATION)
#define IED_PROT_PERIOD_S       ((float)IED_PROT_DECIMATION / (float)IED_SAMPLE_RATE_HZ)

/* Synchrophasor reporting: 60 frames/s, one per nominal cycle. */
#define IED_PMU_RATE_HZ         60u
#define IED_PMU_DECIMATION      (IED_SAMPLE_RATE_HZ / IED_PMU_RATE_HZ)

/* Communications / housekeeping slot: 100 Hz is not a divisor of 1920, so the
 * background slot runs every 16 samples (120 Hz). */
#define IED_COMMS_DECIMATION    16u

/* Target CPU: STM32F407VG, Cortex-M4F at 168 MHz. */
#define IED_CPU_HZ              168000000u
#define IED_CYCLES_PER_SAMPLE   (IED_CPU_HZ / IED_SAMPLE_RATE_HZ)

/* Analog channels, in ADC scan order. One simultaneous-sampling frame holds
 * the bus VTs, the three phase CTs of each of the four bays and the bay-1
 * residual CT, so every channel shares the same time reference (the frame
 * tick / sample-in-second stamped by the acquisition ISR).
 *
 * Bay 1 is the relay's own feeder: the existing 50/51/21/50BF elements read
 * its channels through the IED_CH_IA..IED_CH_IC / IED_CH_IN aliases. Bays 2-4
 * are the other bus connections (see include/bay_config.h for CT ratios and
 * polarity). No element on main consumes bays 2-4 yet. */
#define IED_NUM_BAYS            4u
#define IED_PHASES              3u

typedef enum {
    IED_CH_VA = 0,
    IED_CH_VB,
    IED_CH_VC,
    IED_CH_B1_IA, IED_CH_B1_IB, IED_CH_B1_IC,
    IED_CH_B2_IA, IED_CH_B2_IB, IED_CH_B2_IC,
    IED_CH_B3_IA, IED_CH_B3_IB, IED_CH_B3_IC,
    IED_CH_B4_IA, IED_CH_B4_IB, IED_CH_B4_IC,
    IED_CH_IN,
    IED_NUM_CHANNELS
} ied_channel_t;

#define IED_CH_IA               IED_CH_B1_IA
#define IED_CH_IB               IED_CH_B1_IB
#define IED_CH_IC               IED_CH_B1_IC
/* Phase `ph` (0..2) current channel of bay `bay` (0..3). */
#define IED_CH_BAY_I(bay, ph)   ((ied_channel_t)(IED_CH_B1_IA + 3u * (uint32_t)(bay) + (uint32_t)(ph)))
/* Channel bit in an input-quality mask. */
#define IED_CH_BIT(ch)          (1u << (uint32_t)(ch))
#define IED_BAY_MASK(bay)       (7u << (IED_CH_B1_IA + 3u * (uint32_t)(bay)))

/* 16-bit simultaneous-sampling ADC (bipolar, +/-32767 counts full scale). */
#define IED_ADC_FULL_SCALE      32767
/* Current front end: the CT's rated primary current maps to this many counts,
 * so full scale is 27.3 x rated and primary amps per count = ct_primary / 1200
 * (shared/relay_math/pu_scale.h). Voltage inputs use a fixed VT scale. */
#define IED_ADC_COUNTS_PER_CT_RATED 1200.0f

/* Worst-case execution budgets in CPU cycles, enforced by the Renode timing
 * run (tools/build_realtime_report.py) and by CI. A task that exceeds its
 * budget fails the build even if every functional test passes. */
#define IED_BUDGET_ACQ_CYCLES       2000u
#define IED_BUDGET_PROT_CYCLES      12000u
#define IED_BUDGET_PMU_CYCLES       30000u
#define IED_BUDGET_COMMS_CYCLES     40000u

#endif /* IED_CONFIG_H */
