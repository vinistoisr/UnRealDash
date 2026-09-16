# Foundation skeleton: doctor gate and command preview only.
[CmdletBinding()]
param(
    [ValidateSet('windows', 'device', 'soak')][string]$Target = 'windows',
    [switch]$WhatIf,
    [string]$DoctorScript = (Join-Path $PSScriptRoot 'doctor.ps1')
)
. (Join-Path $PSScriptRoot 'lib/common.ps1')
$profile = @{ windows = 'workstation'; device = 'device'; soak = 'workstation' }[$Target]
$platform = @{ windows = 'Win64'; device = 'Android'; soak = 'Win64' }[$Target]
exit (Invoke-FoundationScript -Profile $profile -Platform $platform -DoctorScript $DoctorScript -Preview ([bool]$WhatIf))
