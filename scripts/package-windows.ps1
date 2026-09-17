[CmdletBinding()]
param(
    [ValidateSet('Development', 'Shipping')][string]$Configuration = 'Development',
    [switch]$WhatIf,
    [string]$DoctorScript = (Join-Path $PSScriptRoot 'doctor.ps1')
)
. (Join-Path $PSScriptRoot 'lib/package.ps1')
exit (Invoke-SmokePackage -Platform Win64 -Configuration $Configuration -DoctorScript $DoctorScript -Preview ([bool]$WhatIf))
