# CoreMark 1.0 @ 550 MHz — STM32H723ZGT6 (h723-mini)

EEMBC CoreMark 1.0 (stock `coremark_1_0_1` sources), **25,000 iterations**, on
the h723-mini board (STM32H723ZGT6) clocked at **550 MHz** (HSE 25 MHz,
PLL1 M=10 N=220 P=1 → SYSCLK, HCLK 275 MHz, APB1/2/3/4 137.5 MHz, VOS scale 0,
flash latency 3 — clock tree copied verbatim from the working vendor
`1.LED闪烁` project). Compiler-agnostic: the same sources build with either
**GNU arm-none-eabi-gcc** or **armclang** (AC6 / the LLVM embedded toolchain),
selected at configure time.

## Results (measured on hardware, 550 MHz, hard-float, I/D caches on)

| Toolchain  | Flags                                      | CoreMark 1.0 | Iterations/s | Total time |
| ---------- | ------------------------------------------ | ------------ | ------------ | ---------- |
| GCC 15.3.1 | `-Ofast -ffp-contract=fast -funroll-loops` | 2372.59      | 2372.59      | 10.54 s    |

The build prints **`Correct operation validated.`** with the expected CRCs
(seedcrc 0xe9f5, crcfinal 0xcc42).

### vs. the h750-mini reference (@ 480 MHz, same flags, GCC)

| Board / chip | Freq    | CoreMark 1.0 |
| ------------ | ------- | ------------ |
| H750 mini    | 480 MHz | 2070.56      |
| H723 mini    | 550 MHz | 2372.59      |

The H723 scores 1.146× the h750 — exactly the 550/480 MHz clock ratio.

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

Open the USART1 console (`COM46` @ 115200 via the ST-Link V2 VCP). Capture at
least ~11 s so one full (~10 s) run completes and the final
`CoreMark 1.0 : <score> / <compiler> / Static` line is printed.

## Notes

* **SysTick**: `board.c` defines `SysTick_Handler` → `HAL_IncTick()` (same
  requirement as the Dhrystone port — without it the core wedges in the weak
  handler on the first tick).
* **ITERATIONS**: 25,000 (same as the h750 480 MHz port) — at 550 MHz a run
  takes ~10 s, valid (CoreMark rejects runs shorter than 10 s).
* Port uses `SEED_VOLATILE` (fixed volatile seeds, so the known-CRC validation
  still matches), `MEM_LOCATION "Static"`, `HAS_FLOAT 1`, and the CORE_TICKS
  timer is `HAL_GetTick()` (1 ms SysTick).
* Clock config: copied from the vendor `1.LED闪烁` 550 MHz example including the
  4 GB MPU region.
* Console: USART1 (PA9/PA10, AF7) on COM3. The ULINK2 cannot capture SWO, so
  UART is the console.
