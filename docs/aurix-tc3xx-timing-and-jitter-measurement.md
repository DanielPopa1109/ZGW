# AURIX TC3xx: measuring exclusive areas, ISR timing and finding jitter

Practical guide to the on-chip resources of an Infineon AURIX TC3xx (TriCore TC1.6.2P) that let you
measure interrupt-disabled regions ("exclusive areas"), ISR execution time and ISR latency, and
that let you find *where* and *why* timing jitter appears. Everything here is publicly documented
(Infineon *AURIX TC3xx User Manual* Part 1 and Part 2, *TriCore TC1.6.2 Architecture Manual*, the
TC3xx data sheet, iLLD examples, and tool-vendor documentation). Register and bit names below follow the
Infineon register headers; **always confirm exact bit positions and reset values in the manual for
your device variant** (TC375, TC387, TC397, ... differ in number of cores, STMs, DMA channels,
GTM clusters and EVADC groups).

---

## 1. Mental model: what "jitter" is and where it comes from

A periodic activity has a *nominal* release instant `t_n = t_0 + n*T`. The activity actually
starts executing at `t_n + L_n`, where `L_n` is the **release-to-execution latency**. Jitter is the
variation of `L_n` (and, separately, variation of the *execution time* `C_n`, which moves the
*finish* instant).

```
 hardware event  --> interrupt request --> arbitration --> context save --> vector fetch --> first ISR instruction
 (compare match,     (SRC.SRR set)         (ICU, CCPN)     (CSA)            (flash/PSPR)
  DMA done, ...)
 |<----------------------------- L (latency) ------------------------------------------>|<--- C (execution) --->|
```

Contributors to `L` on a TC3xx, roughly in order of how often they dominate:

| Source | Why it delays the ISR |
|---|---|
| **Exclusive areas** (any mechanism that masks interrupts, see Section 4.0; ends up as `ICR.IE = 0` or a raised `ICR.CCPN`) | No request of equal/lower priority is taken while masked. Delay = remaining length of the area. |
| **Running ISRs / tasks with priority >= request** | Request is taken only if `SRPN > ICR.CCPN`. Includes nested higher-priority ISRs arriving meanwhile (interference). |
| **Long non-interruptible instruction sequences** | Context save/restore, `DIV`, load/store-multiple etc. complete before the interrupt is taken. |
| **Interrupt vector / ISR code fetch** | Code in PFLASH: wait states, prefetch-buffer / program-cache misses (first run vs. warm run differ). PSPR (scratchpad) is deterministic. |
| **Bus contention** | DMA, other cores, and peripherals compete on the SRI crossbar/SPB. Peripheral register access is slower than RAM and is bridged over SPB. |
| **Data location** | Local DSPR is deterministic; LMU / other core's DSPR / PFLASH data reads are slower and contended. |
| **Clock/PLL changes, debugger halts** | Time-base-relative measurements get distorted. |
| **Request lost/merged** | If a second request arrives while the first is still pending, the router keeps one pending bit only. |

Contributors to `C` (execution time): cache state, data-dependent branches, contention (as above), being
**preempted by higher-priority ISRs** (wall-clock time includes preemption; CPU-cycle time does not),
interrupts taken inside the ISR, and exclusive areas that stretch the *other* side of the picture.

Analytic cross-check (fixed-priority response-time analysis), useful to sanity-check measurements:

```
R_i = C_i + B_i + sum_{j in hp(i)} ceil(R_i / T_j) * C_j
B_i = longest interrupt-disabled / lower-priority-non-preemptible region that can block i
```

Measurements give you `C`, `B` (max exclusive-area length) and `L`; the formula tells you whether the
observed worst case is plausible or whether something unmeasured is hiding.

---

## 2. Worked example (theoretical): sampled data lost through latency

A periodic **GTM** timer output (TOM/ATOM channel, period `T = 100 us`) triggers an **EVADC**
conversion. A **DMA** channel moves each result into a ring buffer in RAM, hardware-only, no CPU. The same
GTM period also raises an interrupt; the **ISR copies a block from the DMA buffer** into a processing buffer.

```
time -->   0        100       200       300       400
GTM tick   |         |         |         |         |      (hardware, tight)
ADC conv   x         x         x         x         x      (hardware, tight, fixed delay after tick)
DMA write  w0        w1        w2        w3        w4     (hardware, lands in ring slot k mod N)
ISR run         [copy w0..w1]                   <-- nominal: reads while slots still valid

Jitter case: a 250 us interrupt-disabled region (or a higher-priority ISR) begins just before tick 1:
ISR run                                  [copy ... reads slot k..]  
                                         ^ ring slots w0,w1 already overwritten if N is small
```

Failure mode: the *producer* (ADC + DMA) is timing-exact, the *consumer* (ISR) is software and has
`L` that varies. If `L_max > (N-1)*T - copy_time` the consumer reads stale/overwritten data, or reads the
same window twice. The effect is data "under/overlap" that looks like a signal glitch, not a timing bug.

What you must be able to answer with measurements:

1. What is the worst-case and distribution of `L` for that ISR? (Section 5)
2. Which exclusive area / ISR / bus master produced the worst `L`? (Sections 4, 6.3)
3. At the moment the ISR starts, how old is the newest sample and has the DMA lapped the reader? (Section 6.1)
4. Did any request get lost? (Section 6.2)

---

## 3. Resource inventory

### 3.1 Time bases (what you can timestamp with)

| Resource | Width / rate | Access cost | Best used for |
|---|---|---|---|
| **STMx** System Timer (one per CPU; TC3xx has up to 6). Free-running 64-bit counter, read via `STMx_TIM0` (low 32 bit) ... `TIM6` (high bits), `STMx_CAP` latches the upper word on a `TIM0` read. Clocked by `f_STM` (typically up to 100 MHz = 10 ns). | 32-bit window: at 100 MHz wraps in ~42.9 s | Peripheral (SPB) read: tens of CPU cycles | Common wall-clock across ISRs and cores; latency vs. compare value; long intervals (>10 ms). Two compare units `CMP0/CMP1` with interrupts (`ICR`, `ISCR`, `SRC_STMxSR0/1`). |
| **CPU cycle counter `CCNT`** (CSFR, per core, enabled via `CCTRL`) | 31-bit + sticky overflow bit, counts CPU clocks (e.g. 5 ns at 200 MHz) | `mfcr`: ~1-2 cycles | Short regions: exclusive areas, ISR execution time. Lowest-overhead timestamp. |
| **Instruction counter `ICNT`** (CSFR) | 31-bit + sticky overflow | `mfcr` | Instruction count -> `CCNT/ICNT` = CPI; separates "executed more code" from "stalled longer". |
| **`M1CNT/M2CNT/M3CNT`** (CSFR) | Configurable event counters selected by `CCTRL.M1/M2/M3` (e.g. instruction-fetch/cache misses, pipeline stalls, load/store stalls; list depends on core) | `mfcr` | Explaining *why* `C` varied: cache misses vs. memory stalls. |
| **GTM TBU** (Time Base Unit, `TBU_TS0/1/2`, 24/27/48-bit counters from GTM CMU clocks) | Clock set by GTM CMU, can be tied to any ratio | SPB read, or hardware capture | Timestamping *in hardware* without CPU: TIM channels capture TBU on an edge. |
| **GTM TIM channels** | Input capture of TBU on edge; modes include PWM measurement (TPWM), pulse-width/period | Zero CPU cost until read; can be routed to DMA/IRQ | Measuring ISR-entry/exit pulses via a pin or internal loopback with hardware precision. |
| **GTM MCS** (Multi-Channel Sequencer, micro-programmable, own RAM) | GTM-clock-accurate | Runs autonomously | Timestamping/capturing sequences with no CPU involvement; advanced. |
| **GPT12 / CCU6 / ERU** | Classic timers, capture pins | Cheap | Alternative independent time bases or pin-capture channels when GTM is busy. |
| **External**: oscilloscope / logic analyzer on a GPIO | ns resolution, independent of MCU clock | Port `OMR` write = a few SPB cycles | Ground truth to validate on-chip numbers. |

Rule of thumb: **`CCNT` for anything under ~10 ms on one core, STM for anything cross-ISR or cross-core,
GPIO + scope/TIM as independent verification.**

### 3.2 Interrupt system (what tells you about latency and lost requests)

* **Interrupt Router (IR) Service Request Control registers `SRC_xxx`** (one per interrupt node):
  `SRPN` (priority), `TOS` (service provider: CPU0..CPUn or DMA), `SRE` (enable), `SRR` (request pending),
  `CLRR/SETR` (clear/set pending), **`IOV`** (overflow: a new request arrived while `SRR` was still set, i.e.
  a request was merged/lost) with `IOVCLR`, **`SWS`** (software-sticky) with `SWSCLR`.
  `IOV` is the single most useful on-chip indicator of "this ISR was too late for at least one period".
* **`ICR`** (CSFR): `IE` (global enable), `CCPN` (current CPU priority number), `PIPN` (pending interrupt
  priority number). Raising `CCPN` instead of clearing `IE` gives *priority-ceiling* exclusive areas that block
  only lower-priority ISRs; both are measurable the same way.
* **`PCXI.PIE`/`PSW`**: saved interrupt-enable state, needed for correct nested `__disable_and_save()` /
  `__restore()`.
* **`BIV`/`BTV`**: interrupt and trap vector tables; place hot vectors/ISRs in PSPR if latency matters.
* **CSA (context save areas) `FCX/LCX/PCXI`**: free list head and limit. The minimum observed distance to `LCX` is a
  nesting-depth/CSA-exhaustion watermark; deep nesting is a jitter and safety signal.
* **Upper context of the interrupted code**: when an ISR starts, the interrupted code's upper context (including the
  return address in `A11`) is in the CSA chain pointed to by `PCXI`. Reading it from inside a probe ISR tells you
  *where* the CPU was when the interrupt was finally accepted (Section 5.3). Verify context layout in the TriCore
  architecture manual.

### 3.3 DMA (what tells you about data movement and overruns)

* Per-channel **`TSR`** (transaction state register): request-pending / channel-active / done flags and
  **transaction-request-lost (`TRL`)**, i.e. a hardware trigger arrived before the previous transaction finished.
* **`CHCSR`** (`TCOUNT` remaining transfers, `CICH/CBL` ...), `SADR/DADR` (current source/destination address): reading
  `DADR` of the ring-writing channel tells the CPU exactly how many samples the hardware has written - an
  *independent write pointer* you can compare with the software read pointer.
* **`ADICR`**: interrupt control (`INTCT` - interrupt after each move / after the transaction / on wrap), shadow
  control (`SHCT`: double buffering, linked lists), circular-buffer sizes (`SCBE/DCBE`, `CHCFGR`). Double
  buffering and "interrupt when block complete" remove the timer-vs-data race by construction.
* **`ERRSR`**: bus/SRI/RAM/linked-list error flags.
* **DMA triggered by SRC node** (`SRC.TOS = DMA`): the request path of any peripheral interrupt can start a DMA channel.
  A *second* DMA channel triggered by the same event that copies `STMx_TIM0` into a buffer is a **zero-CPU hardware
  timestamper** (a chained channel / linked list can interleave "timestamp, data, timestamp, data").

### 3.4 Peripheral-specific status useful for the example

* **EVADC**: result register `RESx` valid flag (`VF`), wait-for-read mode (`WFR`) in the result control register, result
  FIFO option, request-source/result event flags and their `SRC_EVADC...` nodes. With `WFR` set the ADC holds
  the next conversion until the previous result is read; without it, overwrite is silent unless you check the flags.
  Conversions can be triggered by GTM (gate/trigger multiplexers), which makes the *producer* timing hardware-exact.
* **GTM**: ICM (interrupt concentrator) aggregates sub-module interrupts - check which interrupt node a GTM channel
  finally uses (the SRPN/TOS belongs to the ICM-routed `SRC_GTM...` node). TOM/ATOM output compare, `CM0/CM1`,
  `SR0/SR1` shadow registers; the ARU lets sub-modules exchange data without CPU.

### 3.5 Debug / trace infrastructure (OCDS)

| Capability | Production device (SC) | Emulation Device (ED, `MCDS`) |
|---|---|---|
| Run control, breakpoints, data watchpoints, on-chip trigger sets, break-out signals | Yes (DAP/JTAG, Cerberus) | Yes |
| Debug/performance-counter readout while running, periodic sampling by the debugger | Yes (intrusive but works) | Yes |
| Instruction/data **trace** with timestamps, cycle-accurate profiling, interrupt-region analysis | Typically limited/no | **Yes** (MCDS trace to external tool or on-chip trace RAM) |
| Typical tools | Infineon DAP/MiniWiggler, Lauterbach TRACE32, iSYSTEM winIDEA, PLS UDE, Tasking/HighTec debuggers, AURIX Development Studio | Same tools with ED trace hardware |

If an ED/trace tool is available it is the best way to *observe* jitter without changing it; otherwise use
the software-instrumentation recipes below. Debugger halts stop STM/peripherals depending on `OCS` suspend
configuration - never trust time measurements across a breakpoint.

---

## 4. Measuring exclusive areas

An **exclusive area** is a code region that must not be interrupted (or must not run concurrently) so that shared
data stays consistent. On a TC3xx the region is realised by one of the following mechanisms; each one blocks
interrupt service for as long as it is held, so each one must be measured:

| Mechanism (typical API name) | What it does underneath | Notes |
|---|---|---|
| OS global lock: `DisableAllInterrupts` / `EnableAllInterrupts` | Clears global interrupt enable (`ICR.IE`); not nestable | Blocks everything, including highest-priority ISRs. |
| OS nestable lock: `SuspendAllInterrupts` / `ResumeAllInterrupts` | Same as above but with a nesting counter and saved state; only the outermost pair really masks/unmasks | Measure the **outermost** pair only. |
| OS category-2 lock: `SuspendOSInterrupts` / `ResumeOSInterrupts` | Masks only OS-aware ISRs, typically by raising `ICR.CCPN` to a ceiling | Category-1 ISRs above the ceiling are not blocked: latency differs per ISR. |
| BSW exclusive areas: `SchM_Enter_<Module>_<Area>` / `SchM_Exit_<Module>_<Area>` (or module-specific macros) | Usually map onto one of the OS locks above (or onto a resource/spinlock) | One id per module/area gives free attribution. |
| BSW/driver-private interrupt masking macros (module "disable/enable interrupts" helpers, wrappers around compiler intrinsics) | Direct `ICR.IE`/`CCPN` manipulation, save/restore of previous state | Often missed by OS-level instrumentation. |
| OS resources (`GetResource`/`ReleaseResource`, priority ceiling) | Raise task/ISR priority (`CCPN`) for the holder | Blocks lower priorities only; measure hold time. |
| Spinlocks / atomic read-modify-write (`CMPSWAP.W`, `SWAP.W`, OS spinlock API) | Cross-core mutual exclusion, **no** interrupt masking by itself | Cost is *waiting* time on the other core; measure acquire-to-release and wait time separately. |
| Raw compiler intrinsics / inline asm / direct `ICR` writes | Same hardware effect, invisible to any API-level hook | Found by the latency probe (4.2) and static search (4.3). |

Goal: for every exclusive area know **how long it is held (max, mean, histogram), how often it is entered,
which mechanism and which owner (module/area/call site) it belongs to.**

### 4.1 Method A - instrumented enter/exit with `CCNT` (primary method)

Funnel every mechanism from the table above through one instrumented lock/unlock pair (a macro or inline
function), count **only the outermost level** (`depth` 0->1 and 1->0), and read `CCNT`. Practical hooks:

* Redirect the OS lock APIs (`DisableAllInterrupts`/`EnableAllInterrupts`, `SuspendAllInterrupts`/`ResumeAllInterrupts`,
  `SuspendOSInterrupts`/`ResumeOSInterrupts`) to the wrapper, either inside the OS port layer or with a macro override.
* Redirect `SchM_Enter_*`/`SchM_Exit_*` and BSW-private masking helpers to the same wrapper, passing a **per-area id** so the
  statistics are attributed to module/area, not just to "some lock".
* Keep **one nesting counter and one start timestamp per core**, shared by all mechanisms: a `SchM` area nested inside a
  `SuspendAllInterrupts` pair is one continuous blocked interval, and measuring them separately underestimates it.
* Record the mechanism kind (global mask vs. `CCPN` ceiling vs. resource vs. spinlock) with each sample; the blocked set of ISRs differs.
* For `CCPN`-ceiling mechanisms the blocked ISRs are only those with `SRPN <= ceiling`; note the ceiling alongside the duration.

```c
typedef struct {
    uint32 t0;        /* CCNT at outermost entry */
    uint32 depth;     /* nesting level */
    uint32 maxCycles; /* longest region so far */
    uint32 maxId;     /* region id that produced the max */
    uint32 count;
    uint64 sumCycles;
    uint32 hist[16];  /* log2 histogram */
} EaCoreStat;

static EaCoreStat ea[NUM_CORES];                 /* one per core, in local DSPR */

static inline uint32 Ea_Enter(void)
{
    uint32 icr = __disable_and_save();           /* TASKING intrinsic; returns previous ICR.IE */
    EaCoreStat *s = &ea[core_id()];
    if (s->depth++ == 0) { s->t0 = __mfcr(CPU_CCNT); }
    return icr;
}

static inline void Ea_Exit(uint32 icr, uint32 id)
{
    EaCoreStat *s = &ea[core_id()];
    if (--s->depth == 0) {
        uint32 d = (__mfcr(CPU_CCNT) - s->t0) & 0x7FFFFFFFu;   /* 31-bit counter */
        s->count++; s->sumCycles += d;
        s->hist[31u - (uint32)__clz(d | 1u) /* log2 bucket */ & 0xFu]++;
        if (d > s->maxCycles) { s->maxCycles = d; s->maxId = id; }
    }
    __restore(icr);
}
```

Notes:

* `CCNT` must be enabled (`CCTRL.CE = 1`, supervisor mode) on each core that uses it; `CCNT` is per-core, so
  never subtract values read on different cores.
* Region length in time = `cycles / f_CPU`. Also check `IfxCpu_getPerformanceCounterStickyOverflow` (or the overflow bit
  31) to discard intervals longer than 2^31 cycles (~10.7 s at 200 MHz) - anything near this is a bug anyway.
* The timestamp read happens *before* interrupts are restored, so the measured value includes the read itself (~2 cycles) - negligible; measure
  an empty area once to get the **instrumentation floor** and subtract it.
* Keep stats in local DSPR; update them *inside* the exclusive area (cheap, race-free per core). Publish snapshots
  to a lower-priority task, not from inside the region.
* If the lock macro is a global-priority raise (`CCPN`) rather than `IE`, instrument that variant the same way.
* Record **`id`** (call site, `__LINE__`, or enum) of the worst region so the report names the culprit, not just the number.

### 4.2 Method B - latency probe (finds regions you did not instrument)

Instrumentation misses regions in vendor libraries, assembly, or compiler-generated code. A **highest-priority
periodic STM compare interrupt** measures how late it is served; its maximum latency is a **lower bound of the
longest interrupt-disabled/blocking time anywhere in the system** (Section 5.1 gives the code). If the probe's
max latency exceeds your largest instrumented region, an uninstrumented region or a higher-priority ISR exists.
Then use 5.3 (interrupted-PC sampling) to locate it.

### 4.3 Method C - static search

Find all exclusive-area entry points: OS locks (`DisableAllInterrupts`, `SuspendAllInterrupts`, `SuspendOSInterrupts`, resource and spinlock APIs), `SchM_Enter_*`/`SchM_Exit_*`, BSW interrupt-masking helpers, compiler intrinsics, inline asm and `mtcr ICR`/`CCPN` writes
in source and in the disassembly (`.elf` -> `tricore-objdump`/TASKING `elfdump`/debugger). Compare with the
set covered by Method A. Anything uncovered is a candidate for Method B findings.

### 4.4 Method D - pin toggle + scope/logic analyzer

Set a GPIO via `Pn_OMR` at region entry and clear at exit (outermost level only). A logic analyzer in
persistence/infinite-hold mode shows the longest region and its repetition pattern instantly, and is independent of
on-chip clock assumptions. For automation route the pin back to a **GTM TIM** input (loopback) so hardware measures
the high time (TPWM mode) and a low-priority task reads min/max, with zero CPU overhead per event.

---

## 5. Measuring ISR timing

### 5.1 Latency (release -> first instruction)

Use a **compare-match timer whose expected time is known**: STM `CMPx` holds the exact instant the interrupt
*should* fire.

```c
#define PERIOD_TICKS   (f_STM / 10000u)           /* 100 us probe */

void StmProbe_Isr(void)                           /* highest SRPN on this core; first lines read the time */
{
    uint32 now = STM0_TIM0.U;
    uint32 lat = now - STM0_CMP0.U;               /* ticks late; CMP0 still holds the match value */
    STM0_CMP0.U += PERIOD_TICKS;                  /* re-arm */
    /* clear flag per STM ICR/ISCR description */

    Probe_Record(lat);                            /* min / max / sum / histogram, see below */
}
```

For *any* periodic ISR driven by a timer you control (GTM TOM/ATOM, STM, GPT12, CCU6) the same trick
works: read the timer's own capture/compare or counter value at ISR entry, and subtract the programmed instant.
For GTM, read `TBU_TS0` (or the channel counter) and compare with the channel's `CM0`.

Calibrate the **floor** on an idle system (probe latency with nothing else running). The floor is
vector fetch + context save + prologue. Everything above it is interference.

### 5.2 Execution time (first instruction -> return)

Per ISR, at entry and just before return:

```c
void Isr_X(void)
{
    uint32 c0 = __mfcr(CPU_CCNT), i0 = __mfcr(CPU_ICNT);
    /* ... work ... */
    uint32 cyc = (__mfcr(CPU_CCNT) - c0) & 0x7FFFFFFFu;
    uint32 ins = (__mfcr(CPU_ICNT) - i0) & 0x7FFFFFFFu;
    Isr_RecordExec(ISR_X, cyc, ins);
}
```

* `CCNT` delta is *wall-clock including preemption by higher-priority ISRs*. To separate own execution from
  preemption, count nested-ISR time (accumulate in the inner ISRs' own measurement and subtract), or
  use `ICNT` which only counts executed instructions of whatever is running (also includes the inner ISR's
  instructions, so subtract likewise).
* `cycles / instructions` rising while instructions stay constant = stalls (cache misses, bus contention). Select
  `M1CNT/M2CNT/M3CNT` events (via `CCTRL`) to identify which: instruction cache miss, data access stall, etc.
* Record **min / max / mean / count / log2 histogram** per ISR. The *max* matters for scheduling; the *spread*
  (max - min) is the execution jitter.
* For cross-ISR timelines record `(STM timestamp, ISR id, enter/exit)` into a **lock-free ring buffer in DSPR**
  (single writer per core). A 4096-entry ring with 8-byte entries is sufficient for several ms of history.
  Freeze it (stop writing) on a trigger condition (Section 6.3) to get a *flight-recorder* around the incident.

### 5.3 Where was the CPU when the late interrupt was finally taken?

Inside the probe ISR, when `lat > threshold`, read `PCXI`, follow it to the saved upper context and read the saved
`A11` (return address = interrupted PC). Log it. Map it with the `.map`/`.elf` symbols:

* PC right after the unlock of an exclusive area (a resume/exit call or an `ICR` write) -> an area just ended; the symbol identifies its owner.
* PC inside another ISR body (use `ICR.CCPN`/`PIPN` to confirm) -> priority interference, not an exclusive area.
* PC in a long-latency instruction sequence or flash-resident code -> memory/wait-state effect.

This is statistical, non-intrusive profiling and finds uninstrumented regions.

### 5.4 Hardware-only measurement

Toggle a pin at ISR entry/exit via `Pn_OMR`; capture with **GTM TIM** (TBU timestamps on both edges, or
TPWM for width/period) or with a scope. This gives latency (versus the pin of the event source) and execution time
with ~10 ns resolution and without CPU-side measurement code and without a shared time-base assumption.

---

## 6. Finding jitter and lost data in general

### 6.1 Data-age and lap detection (the ADC/DMA example)

Give the consumer an *independent* view of the producer:

1. **DMA write pointer**: at ISR start read the ring-writing DMA channel's `DADR` and `TCOUNT`. Compute
   `produced = (DADR - base)/sample_size + laps * N`. Compare with `consumed` (the software read index).
   `produced - consumed >= N` means the ring was lapped: **data lost**. `produced - consumed` also quantifies backlog.
2. **In-band timestamps**: a chained DMA channel (same trigger) writes `STMx_TIM0` next to each sample.
   Consumer computes `age = STM_now - sample_stamp`. A rising max age is jitter, visible before data is lost.
3. **Sequence counter**: producer writes a free-running counter (via a second DMA move from a counter register or
   via ADC/GTM channel) beside each sample. Gaps/repeats in the consumer = loss/overlap.
4. **ISR period check**: `STM_now - last_ISR_STM_now` vs. nominal `T` - `|delta - T|` is the ISR-to-ISR jitter, and `delta > N*T`
   with no corresponding IOV/TRL points at a long-blocking region.
5. **ADC flags**: `VF`/wait-for-read state and result/request event flags tell whether a conversion
   completed with the previous result unread.

### 6.2 Hardware lost-event flags (check these after every test run)

| Flag | Meaning | Reset/read |
|---|---|---|
| `SRC_x.IOV` | Interrupt request arrived while previous still pending (ISR was >= one period late) | `IOVCLR` |
| DMA `TSR.TRL` per channel | DMA trigger arrived before previous transaction completed | clear via `TSR` control |
| DMA `ERRSR` | Bus/linked-list/RAM errors | Per-flag clear |
| EVADC result `VF`/overwrite status, request-source overflow | Result overwritten or request lost | Per ADC manual |
| CAN/ETH/other peripheral "message lost"/overflow flags | Same idea for communication peripherals | Per peripheral |
| `CCNT/ICNT` sticky overflow | Measurement itself overflowed | Clear after read |
| Trap counters/CSA depletion trap | Nesting too deep | Debug/trap handler |

Build a small "timing health" module that, after a test, reads and clears all of these and prints them next to
the latency histogram. A passing latency histogram with a set `IOV` is a **failure** (the measurement missed the late event).

### 6.3 Triggered capture ("catch the bad period")

* Software trigger: in the probe, `if (lat > LIMIT) { freeze_ring(); save_interrupted_pc(); set_flag(); }`
  then optionally `__debug()` to halt the debugger exactly at the incident.
* Hardware trigger: **OCDS trigger sets / break-out signal** on a data watch of the "late" flag, or an output pin edge for a scope's
  trigger input.
* ED/MCDS: trigger trace recording on the late-flag write and inspect the preceding instruction/ISR trace.

### 6.4 Multi-core

* STM0..STMn are clocked from the same `f_STM` and leave reset together, so STMs are normally aligned, but
  access latency differs per core and a remote STM read is slow. For cross-core ordering use **one STM (e.g. STM0) read by all
  cores** (cost: SPB round trip), or calibrate offsets with a ping-pong handshake and re-verify.
* `CCNT` is per-core: do not compare across cores. Do not compare cores running at different clock dividers.
* Check which core owns each SRN (`SRC.TOS`) and which cores contend for the same LMU/PFLASH/peripheral; run the probe on each core
  that hosts latency-critical ISRs, not just one.
* Cross-core spinlocks/`CMPSWAP.W`-based exclusive areas add blocking *without* disabling interrupts;
  measure their wait time separately (timestamp before acquire and after acquire).

---

## 7. Measurement pitfalls (read before trusting a number)

1. **Observer effect**: STM/peripheral reads are slow (tens of cycles) and `CCNT` reads are fast. Do not mix them
   inside a nanosecond-scale region; pick `CCNT` there. Calibrate instrumentation cost with an empty region.
2. **Counter width/wrap**: `CCNT/ICNT` 31-bit; STM `TIM0` 32-bit window. Use unsigned subtraction and mask correctly. Use `TIM0`+`CAP` for
   64-bit reads, in the documented order.
3. **Warm vs. cold**: first execution after reset/idle has instruction cache and flash prefetch misses. Record the
   first-iteration value separately; do not let it contaminate (or hide) the steady-state maximum. Also run with caches in the
   *production* configuration.
4. **Frequency changes**: if the PLL/divider changes, convert via current `f_CPU/f_STM`, not constants.
5. **Debugger**: breakpoints, single-step, and some trace modes alter timing; compare with a run with no debugger attached
   (self-recording and reading the results over diagnostics or after the test).
6. **Selection bias**: averages hide jitter. Always store **max, min, count of samples over threshold, and histogram**.
   Run long enough to include rare events (cyclic diagnostics, flash/NVM operations, communication bursts, error
   handling paths).
7. **Load realism**: measure under worst-case system load (all communication active, DMA busy, all cores running), otherwise
   contention is missing.
8. **Probe must outrank everything**: if the latency probe is not the highest priority on its core it measures
   itself being preempted (still useful - then it measures *its* effective latency), but it will not bound exclusive areas.
9. **Interrupt flag clear order**: clear/re-arm before the heavy work, otherwise a pending-flag race can hide or fake a late request.
10. **Privilege/protection**: `CCTRL` write and peripheral access require supervisor mode (or `PSW.IO` set); with MPU/memory
    protection active, grant the instrumentation code access to SFRs and the stats area.

---

## 8. Recipe summary (what to implement, in order)

1. Enable `CCNT/ICNT` on every core; provide a single `Timing_Now()` macro (CCNT) and `Timing_Stamp()` (STM).
2. Wrap interrupt lock/unlock in one instrumented API (Method A) with per-region ids and per-core stats.
3. Add a highest-priority STM latency probe per core (5.1) + interrupted-PC capture (5.3).
4. Add entry/exit measurement macros (5.2) on every ISR; keep a flight-recorder ring.
5. Add the timing-health reader (6.2) for `SRC.IOV`, DMA `TRL/ERRSR`, ADC flags and counter overflows.
6. For every producer/consumer pair (ADC->DMA->ISR style) add lap detection or timestamps (6.1).
7. Validate on-chip numbers once with a GPIO + scope/logic analyzer or GTM TIM loopback (4.4 / 5.4).
8. Re-run under worst-case load; keep reports as `max / p99 / histogram / #over-threshold / flags`, not averages.

## 9. Design-level mitigations once the culprit is known

| Finding | Remedy |
|---|---|
| Long exclusive area | Shorten critical section; use `CMPSWAP.W`/atomic operations or lock-free ring buffers; replace global `IE=0` by `CCPN` ceiling so only conflicting ISRs are blocked; split work. |
| Higher-priority ISR interference | Re-assign `SRPN`, move work to a task, move ISR to another core (`TOS`), rate-limit sources. |
| Flash/cache-induced variance | Place ISR code and vector table in PSPR (or lock in cache), hot data in local DSPR. |
| Consumer too late by design | Make it event-driven: **DMA transaction-complete interrupt** (interrupt after N samples) or **DMA double buffering/linked list** instead of a free timer copy; enlarge the ring (`N*T > L_max + copy`); let DMA do the copy to a buffer the ISR only *reads*. |
| Request lost (`IOV`/`TRL`) | Handle backlog (process `produced - consumed` items per ISR), reduce rate, or shift the work to hardware. |
| Bus contention | Use separate RAM banks/local DSPR, change DMA priority/burst size, avoid LMU/peripheral hot-spots on the latency path. |
| Cross-core skew | Use one master time base, align via GTM/STM compare, avoid shared-resource polling in hard-deadline paths. |

---

## 10. Hunting a rare, slow-to-reproduce timing fault (implementation specification)

Scenario: the code runs correctly for minutes or hours, then a failure appears (corrupted/overlapped data,
missed deadline, glitch). Averages and short test runs show nothing. Logging must not disturb timing, and
logging over a slow channel (e.g. SPI) is only allowed from the **main-loop (background) context**, never from an ISR.
All *measurement arithmetic* must still happen in ISR context, because that is where the timestamps are valid.

### 10.1 Design principle: capture in ISR, report in background

```
 ISR context (cheap, bounded, no I/O)                    background/main context (slow, may block on SPI)
 ------------------------------------                    ---------------------------------------------
 read STM / CCNT / ICNT / flags                          drain event queue + read stat snapshots
 integer subtract, compare, max/min, histogram++   -->   format, rate-limit, send over SPI
 on anomaly: copy a fixed-size record to a ring          convert ticks -> ns / us (division, floating point OK here)
 (single producer, per core, in local DSPR)              detect queue overruns, report "records dropped"
```

Rules for ISR-side code:

* Allowed: reads of STM/`CCNT`/`ICNT`/`M1CNT..M3CNT`/SFR flags, unsigned add/subtract/compare, shifts, table index, stores to DSPR.
* Forbidden: division, floating point, `printf`/string formatting, SPI/UART/CAN/Ethernet calls, locks that can block, calls into libraries of unknown latency.
* Everything the ISR stores is **raw ticks** (STM ticks, CPU cycles, instruction counts). Unit conversion happens later in the background context or on the PC.
* **Bounded cost**: the instrumentation per ISR entry/exit should be a few tens of cycles. Measure that cost (empty ISR with and
  without hooks) and put the number in the report; use it as the instrumentation floor.
* One producer per ring, one ring (or one stat block) per core, in that core's local DSPR; the background reader on the same core drains it.
  Use a single-producer/single-consumer ring (head written only by ISR, tail only by reader, `volatile` + memory barrier / `dsync`), or a
  sequence-counter ("seqlock") for snapshot structs so the reader detects a torn read and retries.

### 10.2 What to record, always on (stage 1: cheap, permanent)

Per monitored ISR/task (the producer chain *and* the consumer ISR in the example), kept as small structs:

| Field | Computed in ISR as | Why |
|---|---|---|
| `count` | `++` | Sanity: activations match expected rate. |
| `latMin/latMax` (ticks) | `now - expected` (expected from the compare register or from `last + T`) | Release-to-execution latency extremes. |
| `execMax` (cycles), `execMin` | `CCNT_exit - CCNT_enter` (31-bit mask) | Execution time extremes. |
| `instrMax` / `stallMax` | `ICNT` delta; `M1CNT..M3CNT` deltas | Separates "more work" from "waited longer". |
| `periodMin/periodMax` | `now - lastNow` (STM) | ISR-to-ISR jitter, detects bunching/gaps. |
| `overThresholdCount` | `if (lat > LAT_LIMIT) ++` | Rare-event counter, survives for hours. |
| `lapMax` / `backlogMax` | `produced - consumed` (DMA write pointer vs. read index) | Direct measure of how close the ring came to loss. |
| `phase` (ticks) | `(STM_ISR - STM_producer_stamp) mod T` | Detects slow drift between two time bases (see 10.6). |
| per-window max | max over the current window (e.g. 1 s), reset by the background reader | Shows *when* the worst case grows, not just the global max. |
| `seq` | incremented once per update | Seqlock for tear-free snapshot reads. |
| `exclusiveAreaMax[id]` + `exclusiveAreaMaxWhen` | From the exclusive-area wrapper (Section 4.1) | Longest blocking per module/area and when it occurred. |
| hardware loss flags | read/clear `SRC.IOV`, DMA `TSR.TRL`/`ERRSR`, ADC overflow flags (Section 6.2) | Proof of lost requests/data. |

Also keep a **system-event marker** API: `Timing_Mark(id)` stores `(STM, id)` into the ring (a few cycles). Call it where
other activity starts/ends (diagnostic request received, NVM/flash operation start/end, bus-off recovery, mode change,
communication burst). Failures that appear "randomly" are often correlated with one of these activities, and the marker
timestamps make that correlation visible.

### 10.3 Anomaly trigger and flight recorder (stage 2: rich data only around the failure)

A continuous high-rate trace is too expensive for SPI; instead keep a **circular flight recorder in RAM** and freeze it when something goes wrong.

* Flight recorder entry (fixed 16 bytes): `{ u32 stm, u16 id/event, u16 core+flags, u32 cycles/value, u32 extra }`.
  Events: ISR enter, ISR exit (with exec cycles), exclusive-area exit (with duration and id), marker, DMA write pointer sample, anomaly.
* Size it to cover the last few ms to a few hundred ms before the failure (compute from event rate; typically 1024-4096 entries per core).
* **Trigger conditions evaluated in ISR with integer compares only** (any of): `lat > LAT_LIMIT`; `exec > EXEC_LIMIT`;
  `period` outside `[T-J, T+J]`; `produced - consumed >= N-1`; `IOV`/`TRL` set; `age > AGE_LIMIT`; exclusive-area duration `> EA_LIMIT`.
* On trigger: write an `ANOMALY` entry (cause bitmask, measured value, limit), capture a **context snapshot** (see below), set `frozen = 1`
  so the ring stops overwriting, keep recording for `POST_TRIGGER_COUNT` more entries (so you see after as well as before), then
  signal the background context. Re-arm only after the background context has drained the capture and reported it.
* If another trigger fires while frozen, only increment a `missedTriggers` counter and `OR` its cause into a mask.
* Context snapshot taken at trigger time (all raw values): `STM`, `CCNT`, `ICNT`, `M1..M3CNT`, `ICR` (`IE`, `CCPN`, `PIPN`),
  interrupted PC (saved upper context `A11`, Section 5.3), `FCX/LCX` CSA watermark, DMA channel `DADR/TCOUNT/TSR`, relevant `SRC` nodes
  (`SRPN/SRR/IOV`), ADC result/valid flags, the producer's and consumer's current indices, and the id and owner of the longest
  exclusive area seen in the current window.

### 10.4 Background (main-context) reporter

* Loop: for each core, if `frozen` -> read capture (seqlock/ring tail), format, send via SPI, clear `frozen`; else every `REPORT_PERIOD`
  (e.g. 1 s) send one compact statistics line (min/max/count/overThreshold/per-window max/flags), then reset the per-window fields.
* Convert ticks to time **here**: `ns = ticks * 1e9 / f`, using the actual configured `f_STM` and `f_CPU`.
* Log **drops explicitly**: ring overflow count, missed triggers, SPI queue overruns. A log with silent drops is not evidence.
* Keep the SPI log rate bounded (token bucket) so logging load itself cannot change system behaviour.
* Also log the **background loop period** (`max` time between iterations): if the background context itself starves, the
  logs will be late and you must know.
* Output format: line-oriented text or fixed binary frames with a header `{magic, version, core, seq, stm_at_send}`; include the clock
  frequencies and configuration (thresholds, ring sizes) in a header frame at boot so logs are self-describing.

### 10.5 Investigation procedure (what to do with the data)

1. **Run stage 1 only** (permanent statistics + `overThresholdCount`, no flight recorder) in the failing scenario until the failure appears. Compare the statistics immediately before the failure with an earlier healthy window: which of `latMax`, `execMax`, `periodMax`, `lapMax`, `phase`, exclusive-area maxima, or a hardware flag moved? That names the *class* of cause.
2. **Enable stage 2** with thresholds set just above the healthy maxima observed in stage 1. Reproduce. The frozen capture shows the sequence of events
   leading to the anomaly: who held an exclusive area, which ISR preempted, what the interrupted PC was, whether the DMA pointer lapped the reader.
3. **Attribute**:
   * large `lat`, interrupted PC just after an exclusive-area exit -> that area is the culprit; its id/module is in the capture.
   * large `lat`, another ISR active -> priority interference; look at the preceding ISR enter/exit entries and its `exec`.
   * normal `lat` but `exec` jumped with `ICNT` constant, `CCNT/ICNT` up, `M1..M3` stall/miss counters up -> memory/cache/bus contention
     (check which other master was active via markers).
   * `exec` jumped and `ICNT` also up -> a different code path was taken (data-dependent); record the input state with the marker `extra` field.
   * everything on the consumer side is normal but the data is wrong -> the producer side (ADC/DMA/GTM) or the phase relation is at fault (10.6).
4. **Confirm** by fixing or bounding the suspected cause, and re-running the same duration; the corresponding counter must stay below threshold.
5. **Prove the instrumentation works**: add a test hook that deliberately holds an exclusive area (or delays the consumer ISR) for a known time
   once, and verify that the system records exactly that anomaly with the right cause and magnitude. Never trust "no anomaly" from
   instrumentation that was not shown to catch an injected one.

### 10.6 Specific "works for a long time, then breaks" causes to test explicitly

* **Counter wrap-around**: STM `TIM0` wraps (32-bit; ~42.9 s at 100 MHz), `CCNT/ICNT` wrap (31-bit; ~10.7 s at 200 MHz), 24-bit GTM counters wrap
  in ms-to-seconds. Both the measurement code and the *application* timing code (timeouts, compare-match re-arming such as `CMP += period`, tick
  differences, signed vs. unsigned compares) must be wrap-safe. A failure with a period like 43 s or 10.7 s points here. Log time of failure
  against these periods.
* **Slow phase drift between two time bases**: if the producer (e.g. GTM-triggered ADC + DMA) and consumer (e.g. a timer ISR) are derived from different
  clocks or dividers, their phase slowly slips; once the phase reaches a buffer boundary (or an alignment of the consumer with another periodic
  load), the consumer reads too early/late and data under- or overlaps. Record `phase` (10.2) and look for a ramp that
  correlates with the failure time; the failure period is `1 / |f1 - f2|`-like. Fix by deriving both from one source or by making the consumer
  event-driven from the DMA completion (Section 9).
* **Beat with a rare periodic activity** (cyclic diagnostic, NVM write, watchdog service, communication burst): check the marker timeline around each anomaly.
* **Rare code path or rare data value** that lengthens an exclusive area or an ISR: captured by `execMax`/`EA max` plus the `extra` field.
* **Request merging**: a single missed period leaves only `IOV`; make sure the flag is read and cleared periodically, otherwise later events look like old ones.
* **Thermal/clock/voltage effects**: log `f_CPU`/PLL status and, if available, temperature in the periodic statistics line.

### 10.7 Implementation requirements for the code-writing agent

Deliver, as separate small modules with a configuration header (`TIMING_CFG_*` macros so everything compiles out with one switch):

1. `Timing_Clock`: `Timing_Cycles()` (`CCNT`), `Timing_Instr()` (`ICNT`), `Timing_Stm()` (`STM TIM0`), `Timing_Stm64()` (background only, `TIM0`+`CAP` order),
   `Timing_EnableCounters()` per core, tick->ns converters (background only), wrap-safe delta helpers.
2. `Timing_Stats`: per-core, per-id stat blocks (Section 10.2) with seqlock, ISR-safe update functions, background snapshot/reset functions.
3. `Timing_Ring`: single-producer/single-consumer per-core ring, freeze/post-trigger logic, `Timing_Mark()`.
4. `Timing_Isr` hooks: `TIMING_ISR_ENTER(id)` / `TIMING_ISR_EXIT(id)` macros (CCNT/ICNT/STM reads, latency vs. expected, period, trigger evaluation),
   usable inside existing ISRs with minimal edits.
5. `Timing_ExclusiveArea`: wrapper used by all interrupt-masking mechanisms (Section 4.1) with per-core nesting, per-area ids and attribution.
6. `Timing_Probe`: optional highest-priority STM compare latency probe per core (Section 5.1) + interrupted-PC capture (Section 5.3).
7. `Timing_HwFlags`: read/clear of `SRC.IOV`, DMA `TSR.TRL`/`ERRSR`, ADC overflow flags, DMA write-pointer sampling for lap/backlog.
8. `Timing_Report` (background only): periodic statistics line, anomaly capture dump, drop counters, boot header with clocks and config, token-bucket rate limiting.
9. `Timing_SelfTest`: fault injection hook (hold an exclusive area / delay a consumer ISR by a configurable time), and an overhead measurement routine.

Acceptance criteria:

* No division, floating point, I/O or blocking call in any ISR-context function (verify by code review and by checking the disassembly of hooks).
* Measured per-ISR hook overhead documented; total instrumentation load below an agreed percentage of the CPU budget.
* All shared structures are single-writer or seqlock-protected; no ISR waits on the background context.
* Wrap-around handled everywhere (unit-test the delta helpers with values around 0, 2^31 and 2^32).
* Injected anomaly is detected, correctly attributed (type and magnitude within the instrumentation floor), and reported exactly once with context.
* Logs contain explicit drop counters, configuration header and clock frequencies; no silent loss.
* The feature compiles out completely (zero code, zero RAM) when disabled.
* Build documentation of the thresholds: each limit is justified from measured healthy maxima, not guessed.

---

## 11. Where to read more (public)

* Infineon AURIX TC3xx User Manual, Part 1 (architecture) and Part 2 (peripherals); relevant chapters:
  *CPU / Core SFRs (ICR, BIV, BTV, CCTRL, CCNT, ICNT, M1CNT..M3CNT, FCX, LCX, PCXI)*, *System Timer (STM)*,
  *Interrupt Router (IR) - SRC registers (SRPN, TOS, SRE, SRR, IOV, SWS)*, *Direct Memory Access (DMA) - TSR, CHCSR, ADICR, ERRSR*,
  *Generic Timer Module (GTM) - TBU, TIM, TOM/ATOM, ARU, ICM, MCS*, *Enhanced Versatile ADC (EVADC)*, *Ports (OMR)*,
  *On-Chip Debug Support (OCDS, Cerberus, MCDS on Emulation Devices)*, *Memory maps / PFLASH wait states and prefetch/cache*.
* Infineon *TriCore TC1.6.2 Architecture Manual* (interrupt entry sequence, context model, interrupt enable/disable instructions, upper/lower context layout).
* Infineon iLLD (AURIX Development Studio): `IfxStm`, `IfxCpu` (performance counter functions), `IfxSrc`, `IfxDma`, `IfxGtm_Tom/Tim/Tbu`, `IfxEvadc` examples.
* Tool documentation for TRACE32 (Lauterbach), winIDEA (iSYSTEM), UDE (PLS), AURIX Development Studio: performance-counter and trace/profiling
  chapters (with Emulation Device).
* Fixed-priority response-time analysis (Audsley, Burns, Wellings) for validating measured worst-case latency.
