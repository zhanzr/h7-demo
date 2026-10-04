# hse_test: check whether the h723-mini's 25 MHz HSE crystal locks.

Runs from the internal HSI (64 MHz) only, so it works regardless of HSE. It
enables HSE with a bounded wait and reports READY or FAIL on the USART1 console.

Build & flash:

```bash
bash build.sh                        # -> build/hse_test.hex
cd build && ninja flash              # probe-rs -> internal flash (ST-Link V2)
```

Console (`COMxx`, 115200): `HSE: READY - crystal OK` or `HSE: FAIL - HSERDY never set`.
