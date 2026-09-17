# Builds project targets after the platform-specific doctor gate.
[CmdletBinding()]
param(
    # Platform names remain accepted here for chunk 01 preview callers.
    [ValidateSet('editor', 'game', 'win64', 'android', 'linux')][string]$Target = 'game',
    [ValidateSet('Win64', 'Android', 'Linux')][string]$Platform = 'Win64',
    [ValidateSet('Development', 'Shipping')][string]$Configuration = 'Development',
    [switch]$Isolate,
    [switch]$WhatIf,
    [string]$DoctorScript = (Join-Path $PSScriptRoot 'doctor.ps1')
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'lib/common.ps1')
if ($Target -in @('win64', 'android', 'linux')) {
    if ($PSBoundParameters.ContainsKey('Platform') -and $Platform -ne $Target) {
        Write-Host 'Legacy platform target conflicts with -Platform.'
        exit 1
    }
    $Platform = @{ win64 = 'Win64'; android = 'Android'; linux = 'Linux' }[$Target]
    $Target = 'game'
}
if ($Target -eq 'editor' -and $Platform -ne 'Win64') {
    Write-Host 'Editor builds require -Platform Win64.'
    exit 1
}
if ($Target -eq 'editor' -and $Configuration -eq 'Shipping') {
    Write-Host 'Unreal editor targets do not support Shipping. Use Development.'
    exit 1
}
if ($Isolate -and $Platform -ne 'Win64') {
    Write-Host 'Toolchain isolation requires -Platform Win64.'
    exit 1
}
$profile = @{ Win64 = 'workstation'; Android = 'android'; Linux = 'linux' }[$Platform]
$scriptRoot = Split-Path $PSScriptRoot -Parent
$pinsData = Get-Content (Join-Path $PSScriptRoot 'pins.json') -Raw | ConvertFrom-Json
$previewTarget = if ($Target -eq 'editor') { 'UnRealDashEditor' } else { 'UnRealDash' }
$previewCommand = '& "{0}" {1} {2} {3} -project="{4}" -waitmutex' -f `
    (Join-Path $pinsData.engine.root 'Engine/Build/BatchFiles/Build.bat'), `
    $previewTarget, $Platform, $Configuration, `
    (Join-Path $scriptRoot 'runtime/UnRealDash/UnRealDash.uproject')
if ($WhatIf) {
    exit (Invoke-FoundationScript -Profile $profile -Platform $Platform -DoctorScript $DoctorScript -Preview $true -CommandLine $previewCommand)
}
$savedEnvironment = @{}
$isolationRoot = $null
$code = 1
try {
    if ($Isolate) {
        $isolationRoot = Join-Path ([IO.Path]::GetTempPath()) ('UnRealDash-isolate-' + [guid]::NewGuid().ToString('N'))
        foreach ($name in @('ANDROID_HOME', 'ANDROID_SDK_ROOT', 'NDKROOT', 'NDK_ROOT', 'JAVA_HOME', 'LINUX_MULTIARCH_ROOT')) {
            $savedEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
            $empty = Join-Path $isolationRoot $name
            [void](New-Item -ItemType Directory -Path $empty -Force)
            [Environment]::SetEnvironmentVariable($name, $empty, 'Process')
            Write-Host "Isolation: $name=$empty (empty)"
        }
    }
    Write-Host "Doctor profile: $profile"
    & (Join-Path $PSHOME 'pwsh.exe') -NoProfile -File $DoctorScript -Profile $profile | Out-Host
    if ($LASTEXITCODE -ne 0) {
        Write-Host 'Doctor failed; stopping.'
    } else {
        $root = Split-Path $PSScriptRoot -Parent
        $pins = Get-Content (Join-Path $PSScriptRoot 'pins.json') -Raw | ConvertFrom-Json
        $buildBatch = Join-Path $pins.engine.root 'Engine/Build/BatchFiles/Build.bat'
        $project = Join-Path $root 'runtime/UnRealDash/UnRealDash.uproject'
        $buildTarget = if ($Target -eq 'editor') { 'UnRealDashEditor' } else { 'UnRealDash' }
        Write-Host ('& "{0}" {1} {2} {3} -project="{4}" -waitmutex' -f $buildBatch, $buildTarget, $Platform, $Configuration, $project)
        & $buildBatch $buildTarget $Platform $Configuration "-project=$project" -waitmutex | Out-Host
        $code = $LASTEXITCODE
    }
} catch {
    Write-Host "Build failed: $($_.Exception.Message)"
} finally {
    foreach ($name in $savedEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($name, $savedEnvironment[$name], 'Process')
    }
    if ($isolationRoot -and (Test-Path -LiteralPath $isolationRoot)) {
        $resolved = [IO.Path]::GetFullPath($isolationRoot)
        $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
        if ($resolved.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase) -and
            ([IO.Path]::GetFileName($resolved) -like 'UnRealDash-isolate-*')) {
            Remove-Item -LiteralPath $resolved -Recurse -Force
        }
    }
}
exit $code
