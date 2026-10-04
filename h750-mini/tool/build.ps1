#!/usr/bin/env pwsh
# Build the h750-mini cmake project (arm-none-eabi-gcc + Ninja).
# Windows-specific tool paths below; override via env: $env:CMAKE, $env:NINJA.
$ErrorActionPreference = "Stop"

$Root   = Split-Path -Parent $PSScriptRoot
$Proj   = Join-Path $Root "bare\ov5640_to_st7789"
$Cmake  = if ($env:CMAKE) { $env:CMAKE } else { "C:\Users\user1\.mcuxpressotools\cmake-3.30.0-windows-x86_64\bin\cmake.exe" }
$Ninja  = if ($env:NINJA) { $env:NINJA } else { "D:\Program Files\Meson\ninja.exe" }

Push-Location $Proj
try {
    & $Cmake -G Ninja -DCMAKE_MAKE_PROGRAM="$Ninja" -B build
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $Cmake --build build
    exit $LASTEXITCODE
} finally {
    Pop-Location
}
