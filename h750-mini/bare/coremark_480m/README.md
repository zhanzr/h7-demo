# CoreMark 1.0 @ 480 MHz — STM32H750VBT6 (h750-mini)

EEMBC CoreMark 1.0 (stock `coremark_1_0_1` sources), **25,000 iterations**, on
the h750-mini board (STM32H750VBT6) clocked at **480 MHz** (HSE 25 MHz,
PLL1 M=5 N=192 P=2 → SYSCLK, HCLK 240 MHz, APB1/2/3/4 120 MHz, VOS scale 0,
flash latency 4 — clock tree copied verbatim from the working
`ov5640_to_st7789` project). Compiler-agnostic: the same sources build with
**GNU arm-none-eabi-gcc** (default), **armclang** (Keil AC6) or **starm-clang**
(ST Arm Clang), selected at configure time with `-DSTM32_TOOLCHAIN=`.

## Results (measured on hardware, 480 MHz, hard-float, I/D caches on)

| Toolchain           | Flags                                          | CoreMark 1.0 | Total time |
| ------------------- | ---------------------------------------------- | ------------ | ---------- |
| GCC 15.3.1          | `-Ofast -ffp-contract=fast -funroll-all-loops` | 2127.84      | 11.749 s   |
| GCC 15.3.1          | + `-DSTM32_LTO=ON`                             | 1996.01      | 12.525 s   |
| armclang 6.24 (AC6) | `-Ofast -ffp-contract=fast -funroll-loops`     | 2090.48      | 11.959 s   |
| armclang 6.24 (AC6) | `-Omax -fno-lto` (via `BENCH_OPT_C`)           | **2494.51**  | 10.022 s   |
| ST Arm Clang 21.1.1 | `-Ofast -ffp-contract=fast -funroll-all-loops` | 1801.93      | 13.874 s   |

Every row prints **`Correct operation validated.`**, and the console reports the
flags actually used (`Compiler flags : ...`): `FLAGS_STR` is generated from
`BENCH_OPT` + `BENCH_OPT_C`, so the printed line cannot drift from the build.

> CoreMark scores are the standard single-context numbers; with
> `default_num_contexts = 1` the CoreMark 1.0 column is also the iterations/s
> rate (25,000 iterations per run, ~10–14 s each).

The external-flash (QSPI) twin runs the same five configurations from the
W25Q64 — see `../../app_qspi/coremark_480m/README.md`.

### Where the flags come from

The flags and the `BENCH_OPT` / `BENCH_OPT_C` / `STM32_LTO` knobs are borrowed
from the nano-f411 **f4-demo** benchmarks. `FLAGS_STR` is generated from
`BENCH_OPT` + `BENCH_OPT_C`, so CoreMark prints the flags it was really built
with.

**LTO is a regression on this board — keep it off.** `-DSTM32_LTO=ON` measures
1996.01 it/s, **6.2 % slower** than the non-LTO GCC build (2127.84), the
opposite of the F411 (+6.6 % there), so this is board/toolchain specific rather
than a general rule. Both configurations still validate.

### Toolchains: gcc (default) + two optional

`-DSTM32_TOOLCHAIN=<gcc|armclang|starm-clang>` selects the compiler. All three
build this benchmark; all three results are in the table above.

* **armclang (Keil AC6 6.24)** — fastest CoreMark here: `-Omax` with `-fno-lto`
  reaches **2494.51 it/s**, **+17 %** over the GCC default. `-fno-lto` is
  required because `-Omax` otherwise emits LLVM bitcode that GNU ld cannot
  link; the recipe sets `BENCH_OPT` empty and passes `-Omax -fno-lto` through
  `BENCH_OPT_C`. armclang rejects `-funroll-all-loops`
  (`-Wignored-optimization-argument`), so its rows use `-funroll-loops`.
* **starm-clang (ST Arm Clang 21.1.1, from STM32CubeIDE)** — self-contained
  LLVM 21 + LLD with its own newlib sysroot; slowest on CoreMark (1801.93 it/s,
  **−15 %**). Its newlib keeps `errno` in TLS, so `board/syscalls.c` provides
  the AEABI `__aeabi_read_tp()` shim (unused by the gcc/armclang links, dropped
  by `--gc-sections`).

## Build

Requires the CMake/Ninja environment (MSYS2 mingw64, `build.sh` adds it to
`PATH` automatically). Use a **separate build dir per toolchain** because
`CMAKE_TOOLCHAIN_FILE` is cached after configure.

```bash
# GNU arm-none-eabi-gcc (default)
bash build.sh                      # == cmake -G Ninja .. && ninja

# Keil AC6 (armclang) with its default (armclang-supported) flag set
mkdir -p build-ac6 && cd build-ac6
cmake -G Ninja -DSTM32_TOOLCHAIN=armclang \
      '-DBENCH_OPT=-Ofast -ffp-contract=fast -funroll-loops' ..

# armclang, fastest CoreMark recipe measured above
cmake -G Ninja -DSTM32_TOOLCHAIN=armclang '-DBENCH_OPT=' '-DBENCH_OPT_C=-Omax -fno-lto' ..

# ST Arm Clang (STM32CubeIDE's LLVM 21 + LLD)
mkdir -p build-starm && cd build-starm
cmake -G Ninja -DSTM32_TOOLCHAIN=starm-clang .. && ninja
```

`ninja` builds the `.elf` + `.hex` (the `.hex` is what `ninja flash` programs);
`ninja bin` additionally writes a raw `.bin` image "in case" you need it.

## Flash & measure

```bash
ninja flash        # probe-rs auto-detects the attached probe
                   # (flash-stlink / flash-dap / flash-jlink force one)
```

Capture the USART console on the on-board CH340 (`COM89` here) @ 115200 — run
this from `h750-mini/`:

```bash
python ../tools/serial_capture.py COM89 115200 20
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
* Console: USART1 (PA9/PA10, AF7) via the on-board CH340 (COM89). The ULINK2
  cannot capture SWO, so UART is the console.
