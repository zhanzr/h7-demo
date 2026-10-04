# blink_hello — minimal STM32H750VBT6 (h750-mini) template @ 480 MHz

Tiny project template: blinks the on-board **PA8 LED (LD3)** and prints a
line over USART1 (CH340 → `COM56` @ 115200) on **every toggle** (~1 Hz).
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

You should see one `LED PA8: ON/OFF ...` line per second, alternating, and
the physical LED blinking in sync.
