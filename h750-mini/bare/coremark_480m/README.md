# CoreMark 1.0 @ 480 MHz — STM32H750VBT6 (h750-mini)

EEMBC CoreMark 1.0 (stock `coremark_1_0_1` sources), **25,000 iterations**, on
the h750-mini board (STM32H750VBT6) clocked at **480 MHz** (HSE 25 MHz,
PLL1 M=5 N=192 P=2 → SYSCLK, HCLK 240 MHz, APB1/2/3/4 120 MHz, VOS scale 0,
flash latency 4 — clock tree copied verbatim from the working
`ov5640_to_st7789` project). Compiler-agnostic: the same sources build with
either **GNU arm-none-eabi-gcc** or **armclang** (AC6 / the LLVM embedded
toolchain), selected at configure time.

## Results (measured on hardware, 480 MHz, hard-float, I/D caches on)

| Toolchain          | Flags                                      | CoreMark 1.0 | Iterations/s | Total time |
| ------------------ | ------------------------------------------ | ------------ | ------------ | ---------- |
| GCC 15.3.1         | `-Ofast -ffp-contract=fast -funroll-loops` | 2070.56      | 2070.56      | 12.07 s    |
| armclang 20.0.0git | `-Ofast -ffp-contract=fast -funroll-loops` | 2089.95      | 2089.95      | 11.96 s    |

Both builds print **`Correct operation validated.`** and identical CRC values
(seedcrc 0xe9f5). armclang is ~0.9 % faster than GCC here (closer than in
Dhrystone — CoreMark's guard code is harder to hoist, so the difference is
mostly codegen quality, not optimization artifacts).

> CoreMark scores are the standard single-context numbers; with
> `default_num_contexts = 1` the Iterations/s column equals the CoreMark
> score.

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
```

`ninja` builds the `.elf` + `.hex` (the `.hex` is what `ninja flash` programs);
`ninja bin` additionally writes a raw `.bin` image "in case" you need it.

## Flash & measure

Flash + auto-capture the USART console (COM56, 115200) with the shared tool:

```bash
bash tools/bench_capture.sh h750-mini/coremark_480m/build/coremark_480m.hex 18 coremark-gcc
```

Capture at least ~14 s so one full (≥10 s) run completes and the final
`CoreMark 1.0 : <score> / <compiler> / Static` line is printed.

## Notes

* **SysTick**: `board.c` defines `SysTick_Handler` → `HAL_IncTick()` (same
  requirement as the Dhrystone port — without it the core wedges in the weak
  handler on the first tick).
* **ITERATIONS**: raised from the F407 port's 10,000 to 25,000. CoreMark
  rejects runs shorter than 10 s (`ERROR! Must execute for at least 10 secs`);
  at 480 MHz the F407's 10,000 iterations finish in ~4.8 s, which also made
  `Errors detected` print. 25,000 iterations → ~12 s, valid.
* Port uses `SEED_VOLATILE` (fixed volatile seeds, so the known-CRC validation
  still matches), `MEM_LOCATION "Static"`, `HAS_FLOAT 1`, and the CORE_TICKS
  timer is `HAL_GetTick()` (1 ms SysTick).
* Clock config: copied from `ov5640_to_st7789` (the working reference build)
  including the `__HAL_FLASH_SET_LATENCY(FLASH_LATENCY_4)` + `__DSB()` /
  `__ISB()` barrier and the 4 GB MPU region.
* Console: USART1 (PA9/PA10, AF7) via the on-board CH340 (COM56). The ULINK2
  cannot capture SWO, so UART is the console.
