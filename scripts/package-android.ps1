[CmdletBinding()]
param(
    [ValidateSet('vulkan', 'gles')][string]$Rhi,
    [ValidateSet('Development', 'Shipping')][string]$Configuration = 'Development',
    [switch]$WhatIf,
    [string]$DoctorScript = (Join-Path $PSScriptRoot 'doctor.ps1')
)
if (-not $Rhi) { Write-Host 'The -Rhi switch is required: vulkan or gles.'; exit 1 }
. (Join-Path $PSScriptRoot 'lib/package.ps1')
exit (Invoke-SmokePackage -Platform Android -Configuration $Configuration -Rhi $Rhi -DoctorScript $DoctorScript -Preview ([bool]$WhatIf))
