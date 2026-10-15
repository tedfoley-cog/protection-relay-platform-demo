# Platform figures

Single source of truth: `firmware/include/ied_config.h`. Every number below is a
`#define` there; documentation, tests and the setting tool read the header, not
this page. Change the header first, then this page.

## Measurement chain

| Figure | Value | Header symbol | Notes |
|---|---|---|---|
| Nominal frequency | 60 Hz | `IED_F_NOMINAL_HZ` | 50 Hz is a build-time change, not a setting |
| Sample rate | 1920 Hz (32 samples/cycle) | `IED_SAMPLE_RATE_HZ`, `IED_SAMPLES_PER_CYCLE` | all 16 channels on one timer, one timestamp per frame |
| Analog channels | 16 | `IED_NUM_CHANNELS` | VA VB VC; IA IB IC for bays 1-4; IN (bay-1 residual CT) |
| Bays | 4 | `IED_NUM_BAYS`, `IED_CH_BAY_I(bay, ph)` | bay 1 is the relay's own feeder; bays 2-4 are the other bus connections |
| ADC | 16-bit signed, ±32767 counts | `IED_ADC_FULL_SCALE` | |
| CT scaling | 1200 counts at rated secondary (5 A) | `IED_ADC_COUNTS_PER_CT_RATED` | primary A per count = CT primary / 1200 (`pu_a_per_count`) |
| Phasor estimator | full-cycle DFT, 32 taps | `firmware/src/dsp/phasor.c` | rectangular window; harmonic rejection of integer harmonics; 1-cycle group delay |
| Current pre-filter | digital mimic, X/R = source setting | `firmware/src/ied/ied_app.c` | removes decaying DC on bay-1 currents only |
| Protection pass | 480 Hz (every 4th sample) | `IED_PROT_RATE_HZ`, `IED_PROT_DECIMATION` | 2.083 ms between decisions |
| PMU reporting | 60 frames/s, P class | `IED_PMU_RATE_HZ` | IEEE C37.118.1-2011 + a-2014 |
| Comms task | 120 Hz | `IED_COMMS_RATE_HZ` | DNP3 outstation point refresh |
| Core | STM32F407, Cortex-M4F, 168 MHz | `IED_CPU_HZ` | Renode `stm32f4_discovery` platform |

## Real-time budgets (cycles per pass)

| Task | Rate | Budget | Symbol |
|---|---|---|---|
| Acquisition ISR | 1920 Hz | 2 000 | `IED_BUDGET_ACQ_CYCLES` |
| Protection | 480 Hz | 12 000 | `IED_BUDGET_PROT_CYCLES` |
| PMU | 60 Hz | 30 000 | `IED_BUDGET_PMU_CYCLES` |
| Comms | 120 Hz | 40 000 | `IED_BUDGET_COMMS_CYCLES` |

Measured worst cases per scenario are in `tool/public/realtime.json`
(`python3 tools/build_realtime_report.py`). A new element must leave the
protection task within budget on every scenario; the report fails the build
otherwise.

## Phasor filter response

Full-cycle DFT magnitude response, normalised, for the 32-sample window at 1920 Hz:

| Frequency | Gain |
|---|---|
| DC | 0 |
| 60 Hz | 1.000 |
| 120 Hz (2nd) | 0 |
| 180 Hz (3rd) | 0 |
| 57 Hz / 63 Hz (off-nominal) | 0.996 |
| 55 Hz / 65 Hz | 0.986 |

Decaying DC is not rejected by the DFT; the mimic filter handles it for bay-1
currents. An element on bays 2-4 must state how it handles DC offset
(see `docs/platform/OPEN_QUESTIONS.md`).

## Timing consequences for an element

* A new fault condition is visible in full after one cycle (16.7 ms) of samples;
  decisions on partially filled windows see a ramping magnitude.
* The fastest an element can assert after inception is one protection pass
  after the window fills: ~18.8 ms plus any security count.
* Timers are integer protection passes; `settings_file.c` converts seconds to
  passes with rounding (`K_TICKS_S`).
