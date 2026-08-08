# blink_hello — minimal STM32H723ZGT6 (h723-mini) template @ 550 MHz

Tiny project template: blinks the on-board **PG7 LED (low active)** and prints
a line over USART1 (`COM46` @ 115200 via the ST-Link V2's VCP) on **every
toggle** (~1 Hz). Everything else — 550 MHz clock init, MPU, I/D caches, UART
console, SysTick, newlib stubs, startup + linker script — comes from the
**shared board layer** (`../board`) and toolchain helpers (`../cmake`), so a
new project only needs its own `src/main.c`.

Use this as the starting skeleton for new h723-mini apps: copy the folder,
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
ninja flash        # probe-rs through the ST-Link V2 (SWD) - works
ninja dfu-flash    # USB DFU via STM32CubeProgrammer (BOOT0=1 + reset first)
```

Open `COM46` at 115200 8-N-1 (ST-Link V2 VCP) — you should see one
`LED PG7: ON/OFF ...` line per second, alternating, and the physical LED
blinking in sync.
