function Invoke-SmokePackage {
    param([string]$Platform, [string]$Configuration, [string]$Rhi,
          [string]$DoctorScript, [bool]$Preview)
    $ErrorActionPreference = 'Stop'
    $profile = if ($Platform -eq 'Android') { 'android' } else { 'workstation' }
    Write-Host "Doctor profile: $profile"
    & (Join-Path $PSHOME 'pwsh.exe') -NoProfile -File $DoctorScript -Profile $profile | Out-Host
    if ($LASTEXITCODE -ne 0) { Write-Host 'Doctor failed; stopping.'; return 1 }
    $root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
    $pins = Get-Content (Join-Path $root 'scripts/pins.json') -Raw | ConvertFrom-Json
    $uat = Join-Path $pins.engine.root 'Engine/Build/BatchFiles/RunUAT.bat'
    $project = Join-Path $root 'runtime/UnRealDash/UnRealDash.uproject'
    $archive = Join-Path $root "runtime/UnRealDash/Saved/Packages/$Platform/$Configuration"
    if ($Rhi) { $archive = Join-Path $archive $Rhi }
    $generated = Join-Path $root 'runtime/UnRealDash/Config/GeneratedEngine.ini'
    $arguments = @('BuildCookRun', "-project=$project", '-noP4', "-platform=$Platform",
        "-clientconfig=$Configuration", '-build', '-cook', '-stage', '-pak', '-archive',
        '-package', "-archivedirectory=$archive", '-map=/Game/Smoke/L_Smoke', '-unattended')
    $ini = "[/Script/AndroidRuntimeSettings.AndroidRuntimeSettings]`nbSupportsVulkan=$($Rhi -eq 'vulkan')`nbBuildForES31=$($Rhi -eq 'gles')`n"
    $command = '& "{0}" {1}' -f $uat, (($arguments | ForEach-Object {
        if ($_ -match '^-(project|archivedirectory)=') { $_ -replace '^([^=]+=)(.*)$', '$1"$2"' } else { $_ }
    }) -join ' ')
    if ($Preview) {
        if ($Platform -eq 'Android') { Write-Host "GeneratedEngine.ini preview:`n$ini" }
        Write-Host $command
        return 0
    }
    $ownsGenerated = $false
    try {
        if ($Platform -eq 'Android') {
            if (Test-Path -LiteralPath $generated) { throw "Remove existing override before packaging: $generated" }
            # CreateNew prevents concurrent packages from changing each other's RHI.
            $stream = [IO.File]::Open($generated, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
            $ownsGenerated = $true
            try {
                $bytes = [Text.UTF8Encoding]::new($false).GetBytes($ini)
                $stream.Write($bytes, 0, $bytes.Length)
            } finally { $stream.Dispose() }
        }
        Write-Host $command
        & $uat @arguments | Out-Host
        return $LASTEXITCODE
    } catch {
        Write-Host "Packaging failed: $($_.Exception.Message)"
        return 1
    } finally {
        if ($ownsGenerated) { Remove-Item -LiteralPath $generated -Force }
    }
}

