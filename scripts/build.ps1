# Foundation skeleton: doctor gate and command preview only.
[CmdletBinding()]
param(
    [ValidateSet('win64', 'android', 'linux')][string]$Target = 'win64',
    [switch]$WhatIf,
    [string]$DoctorScript = (Join-Path $PSScriptRoot 'doctor.ps1')
)
. (Join-Path $PSScriptRoot 'lib/common.ps1')
$profile = @{ win64 = 'workstation'; android = 'android'; linux = 'linux' }[$Target]
$platform = @{ win64 = 'Win64'; android = 'Android'; linux = 'Linux' }[$Target]
exit (Invoke-FoundationScript -Profile $profile -Platform $platform -DoctorScript $DoctorScript -Preview ([bool]$WhatIf))
