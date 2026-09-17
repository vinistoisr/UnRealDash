function Resolve-PwshPath {
    if ($PSVersionTable.PSEdition -eq 'Core') {
        $current = (Get-Process -Id $PID).Path
        if ($current -and (Test-Path -LiteralPath $current)) { return $current }
    }
    $command = Get-Command pwsh -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($command -and (Test-Path -LiteralPath $command.Source)) { return $command.Source }
    foreach ($base in @($env:ProgramFiles, $env:LOCALAPPDATA)) {
        if (-not $base) { continue }
        $relative = if ($base -eq $env:ProgramFiles) { 'PowerShell/7/pwsh.exe' } else { 'Microsoft/PowerShell/7/pwsh.exe' }
        $candidate = Join-Path $base $relative
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }
    Write-Host 'PowerShell 7 not found: checked the current Core host, pwsh on PATH, %ProgramFiles%/PowerShell/7/pwsh.exe, and %LOCALAPPDATA%/Microsoft/PowerShell/7/pwsh.exe.'
    return $null
}

function Invoke-SmokePackage {
    param([string]$Platform, [string]$Configuration, [string]$Rhi,
          [string]$DoctorScript, [bool]$Preview, [string]$UatPath)
    $ErrorActionPreference = 'Stop'
    $profile = if ($Platform -eq 'Android') { 'android' } else { 'workstation' }
    Write-Host "Doctor profile: $profile"
    $pwsh = Resolve-PwshPath
    if (-not $pwsh) { return 1 }
    & $pwsh -NoProfile -File $DoctorScript -Profile $profile | Out-Host
    if ($LASTEXITCODE -ne 0) { Write-Host 'Doctor failed; stopping.'; return 1 }
    $root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
    $pins = Get-Content (Join-Path $root 'scripts/pins.json') -Raw | ConvertFrom-Json
    $uat = if ($UatPath) { [IO.Path]::GetFullPath($UatPath) } else { Join-Path $pins.engine.root 'Engine/Build/BatchFiles/RunUAT.bat' }
    $project = Join-Path $root 'runtime/UnRealDash/UnRealDash.uproject'
    $archive = Join-Path $root "runtime/UnRealDash/Saved/Packages/$Platform/$Configuration"
    if ($Rhi) { $archive = Join-Path $archive $Rhi }
    $generated = Join-Path $root 'runtime/UnRealDash/Config/GeneratedEngine.ini'
    # Refuse unsafe paths: general escaping through batch argument parsing is unreliable.
    foreach ($path in @($uat, $project, $archive)) {
        if ($path -match '[&^|<>%]') {
            Write-Host "Packaging refused: path '$path' contains cmd.exe metacharacter '$($Matches[0])'."
            return 1
        }
    }
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
    $result = 0
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
        $result = $LASTEXITCODE
    } catch {
        Write-Host "Packaging failed: $($_.Exception.Message)"
        $result = 1
    } finally {
        if ($ownsGenerated) {
            try { Remove-Item -LiteralPath $generated -Force } catch {
                Write-Host "Cleanup failed for '$generated': $($_.Exception.Message)"
                # Code 2 means packaging succeeded but override cleanup failed.
                if ($result -eq 0) { $result = 2 }
            }
        }
    }
    return $result
}

