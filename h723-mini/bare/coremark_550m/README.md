# CoreMark 1.0 @ 550 MHz — STM32H723ZGT6 (h723-mini)

EEMBC CoreMark 1.0 (stock `coremark_1_0_1` sources), **25,000 iterations**, on
the h723-mini board (STM32H723ZGT6) clocked at **550 MHz** (HSE 25 MHz,
PLL1 M=10 N=220 P=1 → SYSCLK, HCLK 275 MHz, APB1/2/3/4 137.5 MHz, VOS scale 0,
flash latency 3 — clock tree copied verbatim from the working vendor
`1.LED闪烁` project). Compiler-agnostic: the same sources build with either
**GNU arm-none-eabi-gcc** or **armclang** (AC6 / the LLVM embedded toolchain),
selected at configure time.

## Results (measured on hardware, 550 MHz, hard-float, I/D caches on)

| Toolchain  | Flags                                          | CoreMark 1.0 | Iterations/s | Total time |
| ---------- | ---------------------------------------------- | ------------ | ------------ | ---------- |
| GCC 15.3.1 | `-Ofast -ffp-contract=fast -funroll-all-loops` | 2440.93      | 2440.93      | 10.242 s   |
| GCC 15.3.1 | + `-DSTM32_LTO=ON`                             | 2288.54      | 2288.54      | 10.924 s   |

The build prints **`Correct operation validated.`** with the expected CRCs
(seedcrc 0xe9f5, crcfinal 0xcc42) for both rows, and the console reports the
flags actually used (`Compiler flags : ...`), taken from `BENCH_OPT`.

### Where the flags come from

The flags and the `BENCH_OPT` / `BENCH_OPT_C` / `STM32_LTO` knobs are borrowed
from the nano-f411 **f4-demo** benchmarks, which measured `-funroll-all-loops`
~5% faster than `-funroll-loops` on GCC CoreMark. Here it is **+2.9%**
(2372.59 → 2440.93 it/s).

**LTO is a regression on this board — keep it off.** `-DSTM32_LTO=ON` measures
2288.54 it/s, **6.2% slower** than the non-LTO build, the opposite of the F411
(+6.6% there), so this is board/toolchain specific rather than a general rule.
Both configurations still validate.

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

```bash
ninja flash        # probe-rs through the ST-Link V2 (SWD)
```

Open the USART1 console — the board's USB-serial bridge (a CH340; `COM89` on
this machine) @ 115200 — or let the helper do flash + capture in one step:

```bash
PORT=COM89 bash tools/bench_capture.sh \
    h723-mini/bare/coremark_550m/build/coremark_550m.hex 25 coremark-bare
```

Capture at least ~11 s so one full (~10 s) run completes and the final
`CoreMark 1.0 : <score> / <compiler> / Static` line is printed.

## Notes

* **SysTick**: `board.c` defines `SysTick_Handler` → `HAL_IncTick()` (same
  requirement as the Dhrystone port — without it the core wedges in the weak
  handler on the first tick).
* **ITERATIONS**: 25,000 — at 550 MHz a run takes ~10 s, valid (CoreMark
  rejects runs shorter than 10 s).
* Port uses `SEED_VOLATILE` (fixed volatile seeds, so the known-CRC validation
  still matches), `MEM_LOCATION "Static"`, `HAS_FLOAT 1`, and the CORE_TICKS
  timer is `HAL_GetTick()` (1 ms SysTick).
* Clock config: copied from the vendor `1.LED闪烁` 550 MHz example including the
  4 GB MPU region.
* Console: USART1 (PA9/PA10, AF7) via the board's USB-serial bridge (a CH340;
  `COM89` here) @ 115200 — a standalone ST-Link V2 has no virtual COM port.
