# Dhrystone 2.1 @ 480 MHz — STM32H750VBT6 (h750-mini)

Classic Dhrystone 2.1 (dhry_1.c / dhry_2.c / dhry.h), **12,000,000 runs**, on
the h750-mini board (STM32H750VBT6) clocked at **480 MHz** (HSE 25 MHz,
PLL1 M=5 N=192 P=2 → SYSCLK, HCLK 240 MHz, APB1/2/3/4 120 MHz, VOS scale 0,
flash latency 4 — clock tree copied verbatim from the working
`ov5640_to_st7789` project). Compiler-agnostic: the same sources build with
either **GNU arm-none-eabi-gcc** or **armclang** (AC6 / the LLVM embedded
toolchain), selected at configure time.

## Results (measured on hardware, 480 MHz, hard-float, I/D caches on)

Normal toolchain comparison (no LTO):

| Toolchain                | Flags                                      | Dhrystones/s | DMIPS/MHz |
| ------------------------ | ------------------------------------------ | ------------ | --------- |
| GCC 15.3.1               | `-Ofast -ffp-contract=fast -funroll-loops` | 2,296,651    | 2.723     |
| armclang 20.0.0git       | `-Ofast -ffp-contract=fast -funroll-loops` | 2,474,227    | 2.934     |

All builds print correct final values (Int_Glob=5, Arr_2_Glob = runs+10, …)
and each run exceeds the 2 s `Too_Small_Time` gate (measured ~5.2 s GCC /
~4.9 s armclang).

> ⚠ **Do not use LTO for Dhrystone.** GCC `-flto` sees the whole program and
> hoists loop-invariant work out of the timed loop, inflating the score
> **2.13× to 4,897,959 Dhrystones/s (5.808 DMIPS/MHz)** while still passing
> the final-value check. The LTO number is meaningless and is **excluded from
> the table above**. This reproduces exactly the artifact already documented
> for the F407 port (2.2× → 770,713 D/s) in
> `D:\stm32f407_manual_prj\dhry_168m\LTO_on_dhrystone.md`. The `build-gcc-lto`
> directory exists only as reproducible evidence of the artifact.

armclang is ~7.7 % faster than plain GCC here (0.404 µs/run vs 0.435 µs/run).

### vs. the F407 reference (@ 168 MHz, same flags, GCC)

| Board / chip  | Freq    | Dhrystones/s | DMIPS/MHz |
| ------------- | ------- | ------------ | --------- |
| F407 custom   | 168 MHz | 351,370      | 1.190     |
| H750 mini     | 480 MHz | 2,296,651    | 2.723     |

The H750 scores 6.5× the Dhrystones/s — 2.86× from the higher clock and a
further ~2.3× from the M7 core (wider superscalar pipeline, better branch
handling, I/D caches) running the same optimized code.

## Build

Requires the CMake/Ninja environment (MSYS2 mingw64, `build.sh` adds it to
`PATH` automatically). Use a **separate build dir per toolchain** because
`CMAKE_TOOLCHAIN_FILE` is cached after configure.

```bash
# GNU arm-none-eabi-gcc (default)
bash build.sh                      # == cmake -G Ninja .. && ninja

# Keil AC6 (armclang)
mkdir -p build-ac6 && cd build-ac6
cmake -G Ninja -DSTM32_TOOLCHAIN=armclang ..
ninja

# GNU gcc + LTO (kept only as reproducible evidence of the artifact)
mkdir -p build-gcc-lto && cd build-gcc-lto
cmake -G Ninja -DSTM32_LTO=ON ..
ninja
```

`ninja` builds the `.elf` + `.hex` (the `.hex` is what `ninja flash` programs);
`ninja bin` additionally writes a raw `.bin` image "in case" you need it.

## Flash & measure

Flash + auto-capture the USART console (COM56, 115200) with the shared tool:

```bash
bash tools/bench_capture.sh h750-mini/dhry_480m/build/dhry_480m.hex 16 dhry-gcc
```

The console prints the Dhrystones/s and DMIPS/MHz lines every ~5 s; capture a
few seconds longer than one full run to get a clean result line.

## Notes

* **SysTick**: `board.c` defines `SysTick_Handler` → `HAL_IncTick()`. Without
  it the SysTick (enabled by `HAL_Init`) jumps into the startup weak handler
  (an infinite `b .` loop) the moment the first tick fires, so the firmware
  hangs with no output — the very symptom this port originally showed.
* **RUN_NUMBER**: raised from the F407 port's 2,000,000 to 12,000,000 so a
  run at 480 MHz stays comfortably above the 2 s minimum (2M runs at 480 MHz
  complete in ~0.9 s and trip `Measured time too small`).
* Clock config: copied from `ov5640_to_st7789` (the working reference build)
  including the `__HAL_FLASH_SET_LATENCY(FLASH_LATENCY_4)` + `__DSB()` /
  `__ISB()` barrier and the 4 GB MPU region.
* Console: USART1 (PA9/PA10, AF7) via the on-board CH340 (COM56). The ULINK2
  cannot capture SWO, so UART is the console.
