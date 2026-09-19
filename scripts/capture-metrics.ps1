# PLAN 4.8 launch measurement, plus PLAN 4.7's flag gate and foundation previews.
[CmdletBinding()]
param(
    [ValidateSet('windows', 'device', 'soak')][string]$Target = 'windows',
    [switch]$WhatIf,
    # PLAN 4.7 puts the per-flag tests here. -Smoke runs the documented command-line surface
    # against the packaged player: every flag once, both rejections, and the reproduction check.
    [switch]$Smoke,
    [switch]$Launch,
    [string]$Player = (Join-Path $PSScriptRoot '../runtime/UnRealDash/Saved/StagedBuilds/Windows/UnRealDash.exe'),
    [string]$Package = (Join-Path $PSScriptRoot '../tests/fixtures/packages/dials-stage0.udash'),
    [string]$WorkDirectory = (Join-Path $PSScriptRoot '../.tmp/launch-metrics'),
    [ValidateRange(1, 86400)][int]$Seconds = 60,
    [string[]]$PlayerArguments = @('-udash-sweep=60'),
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

if ($Launch) {
    if ($Target -eq 'device') {
        Write-Host 'Android launch capture needs a device harness; in-process fields are implemented but unverified.'
        exit 2
    }
    $ErrorActionPreference = 'Stop'
    try {
        $playerPath = [IO.Path]::GetFullPath($Player)
        $packagePath = [IO.Path]::GetFullPath($Package)
        $workPath = [IO.Path]::GetFullPath($WorkDirectory)
        if ($WhatIf) {
            Write-Host ('Start-Process "{0}" -udash="{1}" -udash-launch-counter=<QPC> -udash-launch-evidence=<unique JSON file>' -f $playerPath, $packagePath)
            exit 0
        }
        foreach ($path in @($playerPath, $packagePath)) {
            if (-not (Test-Path -LiteralPath $path)) { Write-Host "Missing: $path"; exit 2 }
        }
        New-Item -ItemType Directory -Force -Path $workPath | Out-Null
        $evidencePath = Join-Path $workPath ('launch-' + [Guid]::NewGuid().ToString('N') + '.json')
        $arguments = @('-windowed', '-nosplash', '-unattended', ('-udash="{0}"' -f $packagePath),
            ('-quit-after={0}' -f $Seconds), ('-udash-launch-evidence="{0}"' -f $evidencePath)) + $PlayerArguments
        # Stopwatch is QPC on Windows, the same counter FPlatformTime::Cycles64 reads in the app.
        $launchCounter = [Diagnostics.Stopwatch]::GetTimestamp()
        $process = Start-Process -FilePath $playerPath -ArgumentList ($arguments + ('-udash-launch-counter={0}' -f $launchCounter)) -PassThru -WindowStyle Hidden
        $returnCounter = [Diagnostics.Stopwatch]::GetTimestamp()
        $evidence = [ordered]@{
            launch_counter = [string]$launchCounter
            return_counter = [string]$returnCounter
            counter_frequency = [string][Diagnostics.Stopwatch]::Frequency
        }
        # Publish atomically so the sampler cannot mistake a partially written JSON file for evidence.
        $temporaryPath = $evidencePath + '.tmp'
        [IO.File]::WriteAllText($temporaryPath, ($evidence | ConvertTo-Json), (New-Object Text.UTF8Encoding($false)))
        Move-Item -LiteralPath $temporaryPath -Destination $evidencePath
        $overhead = ($returnCounter - $launchCounter) / [double][Diagnostics.Stopwatch]::Frequency
        Write-Host ('Launch evidence: {0}; counter-read to Start-Process return: {1:F6}s' -f $evidencePath, $overhead)
        $process.WaitForExit()
        exit $process.ExitCode
    } catch {
        Write-Host "Launch capture failed: $($_.Exception.Message)"
        exit 1
    }
}

$profile = @{ windows = 'workstation'; device = 'device'; soak = 'workstation' }[$Target]
$platform = @{ windows = 'Win64'; device = 'Android'; soak = 'Win64' }[$Target]
exit (Invoke-FoundationScript -Profile $profile -Platform $platform -DoctorScript $DoctorScript -Preview ([bool]$WhatIf))
