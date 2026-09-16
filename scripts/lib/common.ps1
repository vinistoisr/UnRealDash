function Invoke-FoundationScript {
    param([string]$Profile, [string]$Platform, [string]$DoctorScript, [bool]$Preview)
    # This guard is duplicated on purpose so each entry point fails clearly under Windows PowerShell 5.1.
    if ($PSVersionTable.PSVersion.Major -lt 7) {
        Write-Host 'PowerShell 7 or later is required. Run this script with pwsh.'
        return 1
    }
    Write-Host "Doctor profile: $Profile"
    $pwsh = Join-Path $PSHOME 'pwsh.exe'
    if (-not (Test-Path -LiteralPath $pwsh)) { $pwsh = Join-Path $PSHOME 'pwsh' }
    & $pwsh -NoProfile -File $DoctorScript -Profile $Profile | Out-Host
    if ($LASTEXITCODE -ne 0) { Write-Host 'Doctor failed; stopping.'; return 1 }
    try {
        $scripts = Split-Path $PSScriptRoot -Parent
        $root = Split-Path $scripts -Parent
        $pins = Get-Content (Join-Path $scripts 'pins.json') -Raw | ConvertFrom-Json
        $uat = Join-Path $pins.engine.root 'Engine/Build/BatchFiles/RunUAT.bat'
        $project = Join-Path $root 'runtime/UnRealDash/UnRealDash.uproject'
        $command = '& "{0}" BuildCookRun -project="{1}" -noP4 -platform={2} -clientconfig=Development -build -cook -stage -pak -archive' -f $uat, $project, $Platform
        if ($Preview) { Write-Host $command; return 0 }
        Write-Host 'Foundation skeleton only. Use -WhatIf to preview the package command. Execution is not implemented.'
        return 1
    } catch { Write-Host "Skeleton failed: $($_.Exception.Message)"; return 1 }
}
