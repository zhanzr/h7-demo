# STM32H750 Camera + LCD Boot Fixes

Status: **working** — LCD displays `Clk:480`, OV5640 camera frames stream to the LCD.

Reset behavior — **all three confirmed working**:
1. **Soft reset** (clicking the Debug button in MDK): works.
2. **Reset button** on the board (NRST): works.
3. **Power off/on**: works.

---

## 1. Symptom

Boot stopped inside `HAL_Init()` → `HAL_InitTick()`:

```c
if((uint32_t)uwTickFreq == 0UL)
{
  return HAL_ERROR;
}
```

`HAL_Init()` returned `HAL_ERROR`, so the system never reached the LCD/camera code.

## 2. Root cause (three interlocking issues)

### 2a. `.data` was placed in D2-domain SRAM that is unclocked after reset

The scatter file `MDK-ARM/scatter/stm32h750vbt.sct` spreads RW/ZI data across
four regions, one of which is D2 SRAM:

```
RW_RWZI_3  __SRAM1_2_3_BASE (__SRAM1_2_3_SIZE)  ; 0x30000000, size 0x48000
```

The linker placed `uwTickFreq` (a `.data` global in `stm32h7xx_hal.c`) at
`0x30000014` in D2 SRAM.

On STM32H7 the D2 SRAM clocks (`RCC->AHB2ENR`, bits `SRAM1EN|SRAM2EN|SRAM3EN`)
are **OFF after reset**. The CubeMX-generated `Core/Src/system_stm32h7xx.c`
only enables them when `DATA_IN_D2_SRAM` is defined (line 86 comment / the
`#if defined(DATA_IN_D2_SRAM)` block at line 270). In this project the macro was
neither uncommented nor present in the Keil compiler defines, so the `.data`
copy could not stick in RAM.

Reference: the vendor project (`board_database/main-stm32h750-cam/ov5640_2_inch_lcd`)
avoids this entirely by keeping all RW/ZI in **DTCM** (`0x20000000`, always
clocked) — see its `STM32H750XBH6.sct`, `RW_IRAM1 0x20000000`.

### 2b. D-Cache kept stale/dirty lines across a debugger soft reset

A Cortex-M7 **soft reset does not disable or flush the caches** (only a
power-on reset does). After a debug session the D-Cache is still enabled with
lines from the previous run.

During startup, `__main` → `__scatterload` copies `.data` **before** `main()`.
With a write-back D-Cache still enabled, those writes landed in cache lines and
**never reached physical RAM**. `HAL_InitTick()` then read the stale RAM value
(`0`) for `uwTickFreq`.

### 2c. The first attempted fix made 2b worse

An earlier fix called `SCB_DisableDCache(); SCB_InvalidateDCache();` at the
start of `main()` (USER CODE 1). That is **after** `__scatterload`, so:

1. the freshly copied `uwTickFreq = 1` was sitting in a dirty cache line,
2. `SCB_DisableDCache()` does **not** flush dirty lines,
3. `SCB_InvalidateDCache()` then **discarded** that dirty line,
4. RAM was never updated → `uwTickFreq` still read `0`.

The invalidation must happen **before** `__scatterload`, i.e. in `SystemInit()`.

## 3. The fix

### 3a. Enable D2 SRAM clock before `.data` is copied

Add `DATA_IN_D2_SRAM` to the Keil C compiler defines:
`MDK-ARM/stm32h750_prj.uvprojx` (target `stm32h750_prj`):

```
<Define>USE_PWR_LDO_SUPPLY,USE_HAL_DRIVER,STM32H750xx,DATA_IN_D2_SRAM</Define>
```

This activates the `SystemInit()` block that sets
`RCC->AHB2ENR |= D2SRAM1EN|D2SRAM2EN|D2SRAM3EN` **before** `__main`.
(Equivalent: uncomment `/* #define DATA_IN_D2_SRAM */` in
`Core/Src/system_stm32h7xx.c:86`.)

### 3b. Drop stale I/D-cache lines in `SystemInit()`, before `__scatterload`

`Core/Src/system_stm32h7xx.c`, at the very start of `SystemInit()` (line 178):

```c
void SystemInit (void)
{
#if defined (DATA_IN_D2_SRAM)
 __IO uint32_t tmpreg;
#endif /* DATA_IN_D2_SRAM */

  /* A debugger soft-reset leaves the M7 D/I-caches ENABLED with stale (possibly
     dirty) lines from the previous run. This runs before __main/__scatterload,
     so the .data copy lands in RAM instead of a stale write-back line -- without
     it, globals like uwTickFreq can read back 0.
     On a true power-on reset the caches are already disabled and empty, so the
     maintenance is skipped (guarding against executing the long set/way loops at
     the power-on reset clock/flash config). */
  if ((SCB->CCR & SCB_CCR_DC_Msk) != 0U)
  {
    SCB_DisableDCache();
    SCB_InvalidateDCache();
  }
  if ((SCB->CCR & SCB_CCR_IC_Msk) != 0U)
  {
    SCB_DisableICache();
    SCB_InvalidateICache();
  }

  /* FPU settings ... */
```

`SystemInit()` runs from the reset handler **before** `__main`/`__scatterload`,
so the `.data` copy now goes straight to RAM (verified in the map: flash
`0x0800d374` → RAM `0x30000014`, initializer `01 00 00 00`). The cache
maintenance is conditional on `CCR.DC/IC`, so it runs only when a prior session
left the caches enabled (soft reset) and is skipped on a clean power-on.

### 3c. (Removed) the misplaced cache-drop in `main()`

`Core/Src/main.c` USER CODE 1 now only carries a note; the actual invalidation
lives in `SystemInit()`. Keeping the calls in `main()` would be a no-op but was
previously harmful.

### 3d. Other boot-reliability fixes already in place

- **480 MHz flash latency** — `main.c` USER CODE Init pre-programs
  `__HAL_FLASH_SET_LATENCY(FLASH_LATENCY_4); __DSB(); __ISB();` before
  `SystemClock_Config()`. This avoids the set+readback race inside
  `HAL_RCC_ClockConfig` on the M7 device bus (the `FLASH->ACR` region is Device
  memory), which otherwise intermittently returned `HAL_ERROR`.
- **printf retarget for the full ARM library** (no MicroLIB) — `main.c`
  USER CODE 0. The full library routes stdout through the `_sys_*` layer
  (semihosting by default), so `printf` hung in `_ttywrch`. The fix defines
  `__use_no_semihosting`, a `FILE` type, and `_sys_open/_sys_write/...` that
  transmit to `huart1` (USART1). Verified binding: `printf` → `_printf_char_file`
  → `fputc` → `HAL_UART_Transmit`.

## 3e. H750 multi-RAM regions — which need processing

The H750 has many RAM blocks with **different power/clock/cache behavior**. Only
**D2 SRAM1/2/3** needs the explicit clock enable. Current build usage from the
map:

| Region | Address | Used by | Clock at reset | Cached by D-Cache? | Processing needed? |
|--------|---------|---------|----------------|--------------------|--------------------|
| ITCM | `0x00000000` | nothing (RAM_CODE empty) | always on (CPU TCM) | no | no |
| DTCM | `0x20000000` | stack, heap | always on (CPU TCM) | no | no |
| AXI SRAM (D1) | `0x24000000` | DCMI camera buffer (RAM1), RW data (RAM2) | always on (no clock gate) | **yes** | no clock enable; cache-coherence caveat below |
| SRAM1/2/3 (D2) | `0x30000000` | **all `.data`/`.bss`** (RW_RWZI_3, 0xBFC) | **OFF** — gated in `RCC->AHB2ENR` | yes | **yes — `DATA_IN_D2_SRAM`** |
| SRAM4 (D3) | `0x38000000` | nothing (RW_RWZI_4 empty) | always on (no clock gate) | yes | no |

Evidence: the HAL/device headers define clock-enable macros **only** for
`D2SRAM1/2/3` (`__HAL_RCC_D2SRAM1_2_3_CLK_ENABLE`). There are no
`AXISRAMEN`/`SRAM4EN` macros, so those blocks have no reset-disabled clock gate.

Current map shows all RW/ZI lives in RW_RWZI_3 (D2 SRAM) — DTCM, AXI RAM2 and
SRAM4 got nothing. That is why `DATA_IN_D2_SRAM` alone made the whole `.data`
copy work.

**Cache-coherence caveat for the camera buffer** (AXI RAM1 `0x24000000`): DCMI
writes frames by DMA into AXI SRAM, which is D-Cache-cacheable. The CPU reads it
in `LCD_CopyBuffer`. If frames ever appear stale/garbled, invalidate the region
before reading:
```c
SCB_InvalidateDCache_by_Addr((uint32_t *)Camera_Buffer, Display_BufferSize);
```
This is currently unnecessary (works as-is) because the CPU never allocates
cache lines for the DMA-only buffer, but keep it in mind if you add CPU writes
to the frame buffer.

## 4. Relationship to STM32CubeMX — regeneration caveats

**Yes, this is CubeMX-related.** All four files involved are generated by
CubeMX and will be **overwritten on code regeneration**:

| File | Fix | Survives CubeMX regen? |
|------|-----|------------------------|
| `MDK-ARM/stm32h750_prj.uvprojx` | `DATA_IN_D2_SRAM` define | No — re-add define (or set "D2 SRAM" in Project Manager, which emits it) |
| `Core/Src/system_stm32h7xx.c` | cache drop in `SystemInit()`; uncomment `DATA_IN_D2_SRAM` | No — re-apply both |
| `Core/Src/main.c` | retarget, flash-latency pre-program | **Yes** — all edits are inside USER CODE blocks |
| `MDK-ARM/scatter/stm32h750vbt.sct` | D2 SRAM region (`RW_RWZI_3`) is the original trigger | No (re-generated); see alternative below |

### Preferred permanent solution

Avoid the whole class of problem by **not putting RW/ZI data in D2 SRAM**,
matching the vendor project: remove `RW_RWZI_3` (SRAM1_2_3) and `RW_RWZI_4`
(SRAM4) from the scatter so all RW/ZI goes to DTCM + AXI SRAM2. Then neither
the D2 SRAM clock nor the `.data`-in-D2 placement matters. DTCM is not cached
on the M7, which also makes data coherent across soft resets.

If you do keep data in D2 SRAM, keep `DATA_IN_D2_SRAM` **and** the
`SystemInit()` cache drop — both are required for reliable debugger resets.

## 5. How to verify

1. Build (full rebuild, `UV4 -r`, expect 0 errors, only the pre-existing
   `L6314W: *.o(RAM_CODE)` scatter warning).
2. Confirm `SystemInit` in the map/listing contains the DCISW/ICIALLU cache
   operations and the `AHB2ENR |= 0xE0000000` D2-SRAM clock write.
3. Confirm `uwTickFreq` map entry: RAM `0x30000014`, load `0x0800d374`,
   initializer `01 00 00 00`.
4. Flash and click **Debug** (soft reset): boot passes `HAL_Init`,
   `SystemClock_Config` (480/240/120), LCD shows `Clk:480`, camera frames
   display. `printf` lines appear on USART1.
5. Toggle power on/off and repeat — must boot identically.
