# =============================================================================
# post_cubemx_restore.ps1
#
# Re-applies the project settings that STM32CubeMX "Generate Code" erases.
# Run this ONCE after every CubeMX regeneration, before building/flashing.
#
#     powershell -ExecutionPolicy Bypass -File .\post_cubemx_restore.ps1
#
# Idempotent: safe to run any number of times. Prints "OK (already set)" for
# settings that are already correct and "PATCH" for ones it fixed.
#
# What it restores:
#   1. AC6 (ARMCLANG V6.24) compiler flags in the .uvprojx
#      (CubeMX regenerates an AC5-style project; only AC6 is installed).
#   2. The DATA_IN_D2_SRAM C define in the .uvprojx.
#      (Required: all .data/.bss live in D2 SRAM via the custom scatter; without
#       the define SystemInit() never clocks D2 SRAM -> boot hangs / uwTickFreq=0.)
#   3. The custom scatter file reference in the .uvprojx.
#      (CubeMX writes <ScatterFile/> empty -> linker falls back to DTCM layout.)
#   4. The ULINK2 flash driver in the .uvprojx (Flash2 = UL2CM3.DLL).
#      (CubeMX resets this to ST-Link/ARMv8-M ULINK2 driver.)
#   5. The ULINK2 debug monitor in the .uvoptx (pMon = UL2CM3.DLL).
#      (CubeMX resets this to ST-Link.)
#   6. The guarded I/D-cache maintenance at the top of SystemInit() in
#      Core/Src/system_stm32h7xx.c. (Soft-reset boot reliability.)
#   7. Cads optimization back to the known-good -O1/O2 setting.
# =============================================================================

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path

$uvprojx = Join-Path $root 'MDK-ARM\stm32h750_prj.uvprojx'
$uvoptx  = Join-Path $root 'MDK-ARM\stm32h750_prj.uvoptx'
$sysinit = Join-Path $root 'Core\Src\system_stm32h7xx.c'

foreach ($p in @($uvprojx, $uvoptx, $sysinit)) {
    if (-not (Test-Path $p)) { throw "Missing file: $p" }
}

# ---------------------------------------------------------------------------
# 1. AC6 compiler flags (.uvprojx)
# ---------------------------------------------------------------------------
$u = Get-Content $uvprojx -Raw
if ($u -match '<uAC6>1</uAC6>') {
    Write-Host 'OK    .uvprojx uAC6=1 (AC6)'
} else {
    if ($u -match '<uAC6>0</uAC6>') {
        $u = $u -replace '<uAC6>0</uAC6>', '<uAC6>1</uAC6>'
        Write-Host 'PATCH .uvprojx uAC6 0 -> 1'
    } else {
        $u = $u -replace '<ToolsetName>ARM-ADS</ToolsetName>',
            "<ToolsetName>ARM-ADS</ToolsetName>`r`n      <pArmCC>6240000::V6.24::ARMCLANG</pArmCC>`r`n      <pCCUsed>6240000::V6.24::ARMCLANG</pCCUsed>`r`n      <uAC6>1</uAC6>"
        Write-Host 'PATCH .uvprojx inserted AC6 (uAC6=1)'
    }
    $dirtyUvprojx = $true
}

# ---------------------------------------------------------------------------
# 2. DATA_IN_D2_SRAM define (.uvprojx)
# ---------------------------------------------------------------------------
if ($u -match 'DATA_IN_D2_SRAM') {
    Write-Host 'OK    .uvprojx DATA_IN_D2_SRAM define'
} else {
    $newDef = $u -replace '<Define>USE_PWR_LDO_SUPPLY,USE_HAL_DRIVER,STM32H750xx</Define>',
        '<Define>USE_PWR_LDO_SUPPLY,USE_HAL_DRIVER,STM32H750xx,DATA_IN_D2_SRAM</Define>'
    if ($newDef -eq $u) { throw 'Could not locate the Cads <Define> in .uvprojx' }
    $u = $newDef
    Write-Host 'PATCH .uvprojx added DATA_IN_D2_SRAM define'
    $dirtyUvprojx = $true
}

# ---------------------------------------------------------------------------
# 3. Custom scatter reference (.uvprojx)
# ---------------------------------------------------------------------------
if ($u -match '<ScatterFile>\.\\scatter\\stm32h750vbt\.sct</ScatterFile>') {
    Write-Host 'OK    .uvprojx scatter reference'
} else {
    $newSc = $u -replace '<ScatterFile\s*/>', '<ScatterFile>.\scatter\stm32h750vbt.sct</ScatterFile>'
    if ($newSc -eq $u) { throw 'Could not locate <ScatterFile/> in .uvprojx' }
    $u = $newSc
    Write-Host 'PATCH .uvprojx scatter -> .\scatter\stm32h750vbt.sct'
    $dirtyUvprojx = $true
}

# ---------------------------------------------------------------------------
# 4. ULINK2 flash driver (.uvprojx)
# ---------------------------------------------------------------------------
if ($u -match '<Flash2>BIN\\UL2CM3\.DLL</Flash2>') {
    Write-Host 'OK    .uvprojx Flash2 = UL2CM3.DLL'
} else {
    $newFl = $u -replace '<Flash2>[^<]*</Flash2>', '<Flash2>BIN\UL2CM3.DLL</Flash2>'
    if ($newFl -eq $u) { throw 'Could not locate <Flash2> in .uvprojx' }
    $u = $newFl
    Write-Host 'PATCH .uvprojx Flash2 -> BIN\UL2CM3.DLL'
    $dirtyUvprojx = $true
}

# ---------------------------------------------------------------------------
# 5. Cads optimization back to known-good level (.uvprojx)
#    Targets only the main Cads block (interw=1); not per-file overrides.
# ---------------------------------------------------------------------------
if ($u -match '(?s)<Cads>\s*<interw>1</interw>\s*<Optim>2</Optim>') {
    Write-Host 'OK    .uvprojx Cads Optim=2'
} else {
    $newOpt = $u -replace '(?s)(<Cads>\s*<interw>1</interw>\s*<Optim>)\d+(</Optim>)', '${1}2${2}'
    if ($newOpt -eq $u) { throw 'Could not locate Cads <Optim> in .uvprojx' }
    $u = $newOpt
    Write-Host 'PATCH .uvprojx Cads Optim -> 2'
    $dirtyUvprojx = $true
}

if ($dirtyUvprojx) {
    [System.IO.File]::WriteAllText($uvprojx, $u, (New-Object System.Text.UTF8Encoding($false)))
}

# ---------------------------------------------------------------------------
# 6. ULINK2 debug monitor (.uvoptx)
# ---------------------------------------------------------------------------
$v = Get-Content $uvoptx -Raw
if ($v -match '<pMon>BIN\\UL2CM3\.DLL</pMon>') {
    Write-Host 'OK    .uvoptx pMon = UL2CM3.DLL'
} else {
    $newPm = $v -replace '<pMon>[^<]*</pMon>', '<pMon>BIN\UL2CM3.DLL</pMon>'
    if ($newPm -eq $v) { throw 'Could not locate <pMon> in .uvoptx' }
    [System.IO.File]::WriteAllText($uvoptx, $newPm, (New-Object System.Text.UTF8Encoding($false)))
    Write-Host 'PATCH .uvoptx pMon -> BIN\UL2CM3.DLL'
}

# ---------------------------------------------------------------------------
# 7. Guarded I/D-cache maintenance in SystemInit() (system_stm32h7xx.c)
# ---------------------------------------------------------------------------
$sys = Get-Content $sysinit -Raw
if ($sys -match 'A debugger soft-reset leaves the M7 D/I-caches ENABLED') {
    Write-Host 'OK    system_stm32h7xx.c cache maintenance'
} else {
    if ($sys -notmatch '  /\* FPU settings') { throw 'Could not locate "FPU settings" anchor in system_stm32h7xx.c' }
    $block = @'
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

  /* FPU settings
'@
    $sys = $sys.Replace('  /* FPU settings', $block)
    [System.IO.File]::WriteAllText($sysinit, $sys, (New-Object System.Text.UTF8Encoding($false)))
    Write-Host 'PATCH system_stm32h7xx.c inserted cache maintenance'
}

Write-Host ''
Write-Host 'Done. Build, then flash/reset with the ULINK2.'
