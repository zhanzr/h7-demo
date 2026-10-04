# Dhrystone 2.1 @ 480 MHz — STM32H750VBT6 (h750-mini)

Classic Dhrystone 2.1 (dhry_1.c / dhry_2.c / dhry.h), **12,000,000 runs**, on
the h750-mini board (STM32H750VBT6) clocked at **480 MHz** (HSE 25 MHz,
PLL1 M=5 N=192 P=2 → SYSCLK, HCLK 240 MHz, APB1/2/3/4 120 MHz, VOS scale 0,
flash latency 4 — clock tree copied verbatim from the working
`ov5640_to_st7789` project). Compiler-agnostic: the same sources build with
**GNU arm-none-eabi-gcc** (default), **armclang** (Keil AC6) or **starm-clang**
(ST Arm Clang), selected at configure time with `-DSTM32_TOOLCHAIN=`.

## Results (measured on hardware, 480 MHz, hard-float, I/D caches on)

Normal toolchain comparison (no LTO), internal flash:

| Toolchain           | Flags                                          | Dhrystones/s | DMIPS/MHz |
| ------------------- | ---------------------------------------------- | ------------ | --------- |
| GCC 15.3.1          | `-Ofast -ffp-contract=fast -funroll-loops`     | 2,296,650.75 | 2.723     |
| armclang 6.24 (AC6) | `-Ofast -ffp-contract=fast -funroll-loops`     | 2,474,226.75 | 2.934     |
| ST Arm Clang 21.1.1 | `-Ofast -ffp-contract=fast -funroll-all-loops` | 2,307,692.25 | 2.736     |

All builds print correct final values (Int_Glob=5, Arr_2_Glob = runs+10, …) and
each run exceeds the 2 s `Too_Small_Time` gate (measured ~5.2 s GCC / ~4.9 s
armclang).

**armclang leads by +7.7 %** over plain GCC here (2,474,226.75 vs 2,296,650.75
Dhrystones/s, 0.404 µs/run vs 0.435 µs/run). starm-clang is level with GCC
(+0.5 %, 2,307,692.25) — the opposite of its CoreMark result
(`../coremark_480m/README.md`). starm-clang's newlib keeps `errno` in TLS, so
`board/syscalls.c` provides the AEABI `__aeabi_read_tp()` shim (unused by the
gcc/armclang links).

> ⚠ **Do not use LTO for Dhrystone.** GCC `-flto` sees the whole program and
> hoists loop-invariant work out of the timed loop, inflating the score
> **2.13× to 4,897,959 Dhrystones/s (5.808 DMIPS/MHz)** while still passing
> the final-value check. The LTO number is meaningless and is **excluded from
> the table above**. This reproduces exactly the artifact already documented
> for the F407 port (2.2× → 770,713 D/s) in
> `D:\stm32f407_manual_prj\dhry_168m\LTO_on_dhrystone.md`. The `build-gcc-lto`
> directory exists only as reproducible evidence of the artifact.

On CoreMark the LTO effect goes the other way — it is a plain **~6.2 %
regression**, not an artifact — see `../coremark_480m/README.md`.

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

# ST Arm Clang (STM32CubeIDE's LLVM 21 + LLD)
mkdir -p build-starm && cd build-starm
cmake -G Ninja -DSTM32_TOOLCHAIN=starm-clang .. && ninja

# GNU gcc + LTO (kept only as reproducible evidence of the artifact)
mkdir -p build-gcc-lto && cd build-gcc-lto
cmake -G Ninja -DSTM32_LTO=ON ..
ninja
```

`ninja` builds the `.elf` + `.hex` (the `.hex` is what `ninja flash` programs);
`ninja bin` additionally writes a raw `.bin` image "in case" you need it.

## Flash & measure

```bash
ninja flash        # probe-rs auto-detects the attached probe
                   # (flash-stlink / flash-dap / flash-jlink force one)

# console capture (CH340, COM89 here) — run this from h750-mini/
python ../tools/serial_capture.py COM89 115200 20
```

The console prints the Dhrystones/s and DMIPS/MHz lines every ~5 s; capture a
few seconds longer than one full run (~5 s) to get a clean result line.

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
* Console: USART1 (PA9/PA10, AF7) via the on-board CH340 (COM89). The ULINK2
  cannot capture SWO, so UART is the console.
