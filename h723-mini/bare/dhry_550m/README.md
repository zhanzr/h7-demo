# Dhrystone 2.1 @ 550 MHz — STM32H723ZGT6 (h723-mini)

Classic Dhrystone 2.1 (dhry_1.c / dhry_2.c / dhry.h), **12,000,000 runs**, on
the h723-mini board (STM32H723ZGT6) clocked at **550 MHz** (HSE 25 MHz,
PLL1 M=10 N=220 P=1 → SYSCLK, HCLK 275 MHz, APB1/2/3/4 137.5 MHz, VOS scale 0,
flash latency 3 — clock tree copied verbatim from the working vendor
`1.LED闪烁` project). Compiler-agnostic: the same sources build with either
**GNU arm-none-eabi-gcc** or **armclang** (AC6 / the LLVM embedded toolchain),
selected at configure time.

## Results (measured on hardware, 550 MHz, hard-float, I/D caches on)

| Toolchain    | Flags                                      | Dhrystones/s | DMIPS/MHz |
| ------------ | ------------------------------------------ | ------------ | --------- |
| GCC 15.3.1   | `-Ofast -ffp-contract=fast -funroll-loops` | 2,631,579    | 2.723     |

All builds print correct final values (Int_Glob=5, Arr_2_Glob = runs+10, …)
and each run exceeds the 2 s `Too_Small_Time` gate (measured ~4.6 s).

> ⚠ **Do not use LTO for Dhrystone.** GCC `-flto` sees the whole program and
> hoists loop-invariant work out of the timed loop, inflating the score. The
> LTO number is meaningless and is **excluded from the table above** (a known
> GCC artifact, not a real measurement — the nano-f411 "f4-demo" repo documents
> the same one in `dhry_100m/LTO_on_dhrystone.md`).

The flags come from the same `BENCH_OPT` / `BENCH_OPT_C` knobs borrowed from
those f4-demo benchmarks. Dhrystone keeps `-funroll-loops`: the F411 measured no
gain from `-funroll-all-loops` for it, and this board's numbers are unchanged by
it (CoreMark, by contrast, gained 2.9% — see `../coremark_550m/README.md`).

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
this machine) @ 115200 — or let the helper flash + capture in one step:

```bash
PORT=COM89 bash tools/bench_capture.sh \
    h723-mini/bare/dhry_550m/build/dhry_550m.hex 20 dhry-bare
```

The console prints the Dhrystones/s and DMIPS/MHz lines every ~4.5 s; capture a
few seconds longer than one full run to get a clean result line.

## Notes

* **SysTick**: `board.c` defines `SysTick_Handler` → `HAL_IncTick()`. Without
  it the SysTick (enabled by `HAL_Init`) jumps into the startup weak handler
  (an infinite `b .` loop) the moment the first tick fires, so the firmware
  hangs with no output.
* **RUN_NUMBER**: kept at 12,000,000 — at 550 MHz a run takes ~4.5 s,
  comfortably above the 2 s `Too_Small_Time` gate.
* **Do not use LTO for Dhrystone**: GCC `-flto` hoists loop-invariant work out
  of the timed loop and inflates the score (a known GCC artifact).
* Clock config: copied from the vendor `1.LED闪烁` 550 MHz example (the working
  reference build) including the 4 GB MPU region.
* Console: USART1 (PA9/PA10, AF7) via the board's USB-serial bridge (a CH340;
  `COM89` here) @ 115200 — a standalone ST-Link V2 has no virtual COM port.
