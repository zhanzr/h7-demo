# blink_hello — minimal STM32H750VBT6 (h750-mini) template @ 480 MHz

Tiny project template: blinks the on-board **PA8 LED (LD3)** (~1 Hz) and
prints the **internal ADC3 channels** over USART1 (CH340 → `COM89` @ 115200)
once per toggle: **die temperature** (`ADC_CHANNEL_TEMPSENSOR`, ADC3_IN18),
the actual **Vref+** derived from the internal bandgap (`ADC_CHANNEL_VREFINT`,
ADC3_IN19), and the **VBAT** pin via `ADC_CHANNEL_VBAT` (ADC3_IN17, internal
/4 divider — pure noise unless a battery sits on the pin). ADC3 runs at
16 bit off its own PLL2 P = 12.5 MHz kernel clock, offset + linearity
calibrated at boot, all three analog paths enabled and channels preselected
(PCSEL — mandatory on the 16-bit H7 ADC IP). Every printed value is a
16-sample trimmed mean, and the boot banner dumps bursts of raw conversions
so reference jitter is visible. The factory calibration words (TS_CAL1/
TS_CAL2 at 0x1FF1E820/840, VREFINT_CAL at 0x1FF1E860) are validated at
runtime; if they are unprogrammed/garbage the maths fall back to the
datasheet typical values (V30 762 mV, 2.5 mV/°C) and the banner says which
source is in use.
Everything else — 480 MHz clock init, MPU, I/D caches, UART console, SysTick,
newlib stubs, startup + linker script — comes from the **shared board layer**
(`../board`) and toolchain helpers (`../cmake`), so a new project only needs
its own `src/main.c`.

Use this as the starting skeleton for new h750-mini apps: copy the folder,
rename the project in `CMakeLists.txt`, and replace `src/main.c`.

## Build

```bash
bash build.sh                      # default: GNU arm-none-eabi-gcc == cmake -G Ninja .. && ninja

# Keil AC6 (armclang) in a separate build dir (CMAKE_TOOLCHAIN_FILE is cached)
mkdir -p build-ac6 && cd build-ac6
cmake -G Ninja -DSTM32_TOOLCHAIN=armclang ..
ninja
```

`ninja` builds the `.elf` + `.hex` (the `.hex` is what `ninja flash` programs);
`ninja bin` additionally writes a raw `.bin` image "in case" you need it.

## Flash + observe

```bash
bash tools/bench_capture.sh h750-mini/blink_hello/build/blink_hello.hex 8 blink
```

You should see the physical LED blinking (~1 Hz) and one line per second:

```
ADC3 cal words: TS_CAL1 12354  TS_CAL2 16311  VREFINT_CAL 24340 (chip rev 0x2003)
ADC3 raw: temp 13055  vref 26710  vbat 1027 | using factory calibration
Tdie 45 C | Vref+ 2710 mV | VBAT 1254 mV (raw t/v/b 15788/29629/7583)
```

`Tdie` ≈ ambient + a few °C of self-heating, `Vref+` ≈ 2500 mV while the
internal VREFBUF is the reference, `VBAT` is noise until a battery is
fitted. Until the board fix below lands, all three values wander because
the reference node moves under them.

### Known board issue: missing VREF+ decoupling capacitor

The h750-mini leaves the **VREF+ pin with no decoupling and not tied to
VDDA**. The ADC reference must exist for *any* conversion (internal or
external; the VREFINT bandgap is a measurement input, not a reference), so
with VREF+ open every conversion read exactly `65535` on every ADC instance.

Firmware-side bring-up (2026-10-23, this board):

Captured with `CHIP=STM32H750VB PORT=COM3 bash tools/bench_capture.sh ...`:

```
VREFBUF CSR before: 00000002
VREFBUF CSR after ENVR=1,HIZ=0 write: 00000001
Vref+ = internal VREFBUF 2500 mV (ready in 0 ms)
ADC3 burst temp: 8319 8255 24576 8223 8319 8223 8223 8207
ADC3 burst vrefint: 22543 32192 22528 22591 21535 21535 24639 21519
```

Live register dump over SWD confirms the digital side is correct:
`RCC->APB4ENR = 0x00018000` (VREFEN set), `VREFBUF->CSR = 0x9`
(ENVR|VRR), `ADC3_COMMON->CCR = 0x00C40000` (PRESC=/2, VREFEN, TSEN),
`SMPR2 = 0x3F000000` (810.5 cycles on IN18/IN19), `PCSEL = 0x000C0000`.
PLL2 is M=5/N=60/P=12/Q=2/R=2 → PLL2_P 25 MHz → ADC 12.5 MHz.

The conversions are still invalid, and the **raw code pattern is the tell**:
before the VREF fix the reading was exactly `65535` (all ones), now it is a
run of trailing ones after a few correct MSBs (`0x200F`, `0x201F`, `0x203F`,
`0x207F`, `0x5800`, ...). That is what an SAR produces when the **reference
collapses during the bit trials** — VREFINT and the temp channel also
disagree, so it is the VREF+ node, not the sensor. `VRR = 1` only means the
buffer thinks it is in regulation; without the load capacitor a low-power
VREFBUF oscillates and cannot supply the SAR's switched-cap charge.

⇒ **Hardware action required on this board**: VREF+ (LQFP100 pin 21) has no
decoupling and is not tied to VDDA. Either add **1 µF + 100 nF from VREF+ to
VSS** and keep the VREFBUF enabled, or tie VREF+ to VDDA/3.3 V and delete the
`VREFBUF_Start()` call (then `g_vref_mv` stays 3300 mV). No firmware change
can compensate for a missing reference reservoir.

`Tdie` ≈ ambient + a few °C of self-heating. At boot the firmware tries the
internal **VREFBUF** (2.5 V) to drive the **VREF+** pin — the ADC reference,
required for *any* conversion (internal or external; the VREFINT bandgap is
only a measurement input, not a reference). The VREFBUF block has its own
clock gate, `RCC_APB4ENR.VREFEN` (bit 15 — *not* `SYSCFGEN`), so
`VREFBUF_Start()` calls `__HAL_RCC_VREF_CLK_ENABLE()` before its first
register access; the HAL VREFBUF helpers assume that gate is already open,
and with it closed `CSR` reads back 0 and the write is discarded (this was
the original "VREFBUF not implemented" failure). It then prints
`Vref+ = internal VREFBUF 2500 mV` and the temp maths use 2500 mV. If it
warns instead, check VDDA and that **VREF+** has its 1 µF + 100 nF
decoupling. `Tdie 1373 C` with `temp 65535` = the raw reading is pinned at
full scale: the board's **VREF+ is open** — tie it to VDDA/3.3 V
(0 Ω + ~100 nF) before trusting any ADC reading.
