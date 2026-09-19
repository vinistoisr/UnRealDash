# Foundation skeleton: doctor gate and command preview, plus PLAN 4.7's flag gate.
[CmdletBinding()]
param(
    [ValidateSet('windows', 'device', 'soak')][string]$Target = 'windows',
    [switch]$WhatIf,
    # PLAN 4.7 puts the per-flag tests here. -Smoke runs the documented command-line surface
    # against the packaged player: every flag once, both rejections, and the reproduction check.
    [switch]$Smoke,
    [string]$DoctorScript = (Join-Path $PSScriptRoot 'doctor.ps1')
)
. (Join-Path $PSScriptRoot 'lib/common.ps1')

if ($Smoke) {
    # The doctor still runs first, because a flag gate against a stale or missing player would
    # report on the wrong binary.
    Write-Host 'Doctor profile: workstation'
    $pwsh = Join-Path $PSHOME 'pwsh.exe'
    if (-not (Test-Path -LiteralPath $pwsh)) { $pwsh = Join-Path $PSHOME 'pwsh' }
    & $pwsh -NoProfile -File $DoctorScript -Profile workstation | Out-Host
    if ($LASTEXITCODE -ne 0) { Write-Host 'Doctor failed; stopping.'; exit 1 }
    $gate = Join-Path $PSScriptRoot 'run-flag-gate.ps1'
    if ($WhatIf) { Write-Host "& `"$gate`""; exit 0 }
    & $gate
    exit $LASTEXITCODE
}

$profile = @{ windows = 'workstation'; device = 'device'; soak = 'workstation' }[$Target]
$platform = @{ windows = 'Win64'; device = 'Android'; soak = 'Win64' }[$Target]
exit (Invoke-FoundationScript -Profile $profile -Platform $platform -DoctorScript $DoctorScript -Preview ([bool]$WhatIf))
