#!/usr/bin/env pwsh
# Flash the h750-mini build to the STM32H750VB via probe-rs (Keil ULINK2/SWD).
# Override via env: $env:PROBE_RS, $env:PROBE, $env:CHIP
$ErrorActionPreference = "Stop"

$Root  = Split-Path -Parent $PSScriptRoot
$Hex   = Join-Path $Root "bare\ov5640_to_st7789\build\ov5640_to_st7789.hex"
$Rs    = if ($env:PROBE_RS) { $env:PROBE_RS } else { "C:\Users\user1\.cargo\bin\probe-rs.exe" }
$Probe = if ($env:PROBE) { $env:PROBE } else { "c251:2722:V0010M9E" }
$Chip  = if ($env:CHIP) { $env:CHIP } else { "STM32H750VB" }

& $Rs download --probe $Probe --chip $Chip --protocol swd `
    --binary-format hex --verify --reset --non-interactive --disable-progressbars $Hex
exit $LASTEXITCODE
