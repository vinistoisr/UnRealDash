# Foundation skeleton: doctor gate and command preview only.
[CmdletBinding()]
param(
    
    [switch]$WhatIf,
    [string]$DoctorScript = (Join-Path $PSScriptRoot 'doctor.ps1')
)
. (Join-Path $PSScriptRoot 'lib/common.ps1')
$profile = 'workstation'
$platform = 'Win64'
exit (Invoke-FoundationScript -Profile $profile -Platform $platform -DoctorScript $DoctorScript -Preview ([bool]$WhatIf))
