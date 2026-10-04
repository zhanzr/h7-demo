# h743_verify — is the "STM32H750VBT6" actually an STM32H743 (2 MB flash)?

Some STM32H750VBT6 chips are believed to be *shadow* STM32H743VIT6 dies: the
same silicon, but marked/binned as the 128 KB H750 part. If the rumor is true,
the **2 Mbyte flash** is physically present and fully usable.

This project tests that on hardware. It deliberately produces a firmware
image **larger than 128 KB** and verifies it reads back.

## What the firmware does

`src/main.c` contains a **15 x 128 KiB** `flash_probe[]` array pinned to flash
address **0x08020000** (sector 1) by `h743_verify.ld`. That linker script
declares `FLASH = 2M` so the image links (an H750-only 128 KB script would
reject it). Sector 0 (0x08000000..0x0801FFFF) holds the running app; the probe
array covers sectors 1..15, i.e. **the entire 2 Mbyte span up to 0x081FFFFF**.
Each 128 KB sector carries its own distinct fill byte (0x51..0x5F), so a short
flash or address aliasing cannot fake the readback. At runtime:

```
=== h743_verify on STM32H750VBTx @ 480000000 Hz ===
FLASH_SIZE register: 128 KB (131072 bytes)              <- factory-programmed size
flash_probe @ 0x08020000, 1966080 bytes = 15 sectors x 128 KB (probe ends at 0x08200000, image > 128 KB)
sector  0 @ 0x08020000: pat 0x51 got 0x00a20000 exp 0x00a20000 -> OK
...
sector 14 @ 0x081e0000: pat 0x5f got 0x00be0000 exp 0x00be0000 -> OK
checksum total: 0x0a500000, 15/15 sectors OK -> FLASH PRESENT (full 2 MB readback OK)
```

- `FLASH_SIZE register` reads the real size the die reports (informational;
  a shadow die may or may not report 2048).
- The per-sector **checksum** lines are the authoritative result: each only
  matches if the flash tool was able to *program* that sector and the part
  *retains* it.

## Measured result (this board, 2026-08)

Flashed with `CHIP=STM32H743VI` (2 MB def); probe-rs download + `--verify` of
the whole ~1.9 MB image **succeeded** (~396 s), and the firmware reported
**15/15 sectors OK** across the full 2 Mbyte span:

```
FLASH_SIZE register: 128 KB (131072 bytes)
flash_probe @ 0x08020000, 1966080 bytes = 15 sectors x 128 KB (probe ends at 0x08200000, image > 128 KB)
sector  0 @ 0x08020000: pat 0x51 got 0x00a20000 exp 0x00a20000 -> OK
...
sector 14 @ 0x081e0000: pat 0x5f got 0x00be0000 exp 0x00be0000 -> OK
checksum total: 0x0a500000, 15/15 sectors OK -> FLASH PRESENT (full 2 MB readback OK)
```

**Verdict: the full 2 Mbyte flash is real and functional** — every sector from
0x08000000 to 0x081FFFFF programs, verifies and reads back correctly. The
`FLASH_SIZE` register still reports 128 KB, i.e. the part is factory-binned as
an H750, but the silicon underneath is the STM32H743VIT6's 2 Mbyte flash —
the "shadow STM32H743VIT6" rumor is **confirmed**.

## Build

```bash
bash build.sh                      # default: GNU arm-none-eabi-gcc
# ninja  -> h743_verify.elf + h743_verify.hex (image > 128 KB)
```

## Run the test

Flash it with the **H743 chip definition** (2 MB) so probe-rs will attempt the
high addresses, and capture the console:

```bash
CHIP=STM32H743VI bash tools/bench_capture.sh \
    h750-mini/bare/h743_verify/build/h743_verify.hex 8 h743-verify
```

### Interpreting the result

| Outcome                                                        | Meaning                                        |
| ------------------------------------------------------------- | ---------------------------------------------- |
| `checksum ... -> FLASH PRESENT (readback OK)`                  | flash beyond 128 KB programs & reads back → the 2 MB flash is really there (**rumor confirmed**) |
| probe-rs download fails with a flash/verify error | write to 0x08020000+ rejected → part really only has 128 KB |
| `checksum ... -> FLASH MISSING (readback mismatch)`           | write "succeeded" but content didn't stick     |

### Control run (optional)

The same image against the official H750 chip definition should be **rejected**
by probe-rs (image exceeds the 128 KB it knows about):

```bash
bash tools/bench_capture.sh h750-mini/bare/h743_verify/build/h743_verify.hex 8 h743-verify-h750
```

That error is expected and only confirms probe-rs enforces the H750's 128 KB —
it says nothing about the physical part by itself.

## Notes

* `FLASH_SIZE` register: `0x1FF1E880` (factory-programmed flash size in KB;
  read via the CMSIS `FLASH_SIZE` macro). The authoritative test is the
  checksum, not this register, because a binned die may still report 128.
* After the test, re-flash a normal app (e.g. `blink_hello`) to restore a
  known-good image.
* Only the `.hex` is produced by default; `ninja bin` writes a raw `.bin`.
