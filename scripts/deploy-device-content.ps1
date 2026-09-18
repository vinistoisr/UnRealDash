# Push staged non-UFS content to the directory the app actually reads.
#
# UAT's deploy step does push non-UFS files, but it pushes them to the legacy
# /sdcard/UnrealGame/<Project> location. This project sets bUseExternalFilesDir=True, which the 4.0
# smoke spike chose deliberately because that tree needs no runtime permission and adb reaches it
# without root, so the player reads from
#   /storage/emulated/0/Android/data/<package>/files/UnrealGame/<Project>
# instead. The two disagree, so files that stage correctly and deploy correctly still never arrive.
#
# Rather than give up bUseExternalFilesDir, this pushes the staged non-UFS files to the location the
# player reads. It takes the file list from Manifest_NonUFSFiles_Android.txt, so it stays correct as
# the set of non-UFS content changes and never hardcodes "the schemas".
[CmdletBinding()]
param(
    [string]$Adb = "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe",
    [string]$Package = 'com.unrealdash.player',
    [string]$Stage = 'runtime/UnRealDash/Saved/StagedBuilds/Android',
    [switch]$WhatIf
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$stagePath = Join-Path $root $Stage
$manifest = Join-Path $stagePath 'Manifest_NonUFSFiles_Android.txt'
if (-not (Test-Path $manifest)) { Write-Output "No non-UFS manifest at $manifest. Package for Android first."; exit 1 }

$target = "/storage/emulated/0/Android/data/$Package/files/UnrealGame/UnRealDash"
$entries = Get-Content $manifest | Where-Object { $_.Trim() } | ForEach-Object { ($_ -split "`t")[0] }
Write-Output ("non-UFS entries in manifest: {0}" -f $entries.Count)

$pushed = 0
$missing = 0
foreach ($rel in $entries) {
    # UECommandLine.txt is the app's runtime command line, not content. The staged copy carries a
    # relative -project= that does not resolve on device, and pushing it produces a "Failed to open
    # descriptor file" dialog on the next launch. Whoever launches the app owns this file.
    if ([IO.Path]::GetFileName($rel) -ieq 'UECommandLine.txt') { continue }
    $source = Join-Path $stagePath $rel
    if (-not (Test-Path $source)) { $missing++; Write-Output "  missing locally: $rel"; continue }
    $destination = "$target/$($rel -replace '\\', '/')"
    if ($WhatIf) { Write-Output "  would push $rel"; $pushed++; continue }
    $parent = $destination.Substring(0, $destination.LastIndexOf('/'))
    & $Adb shell mkdir -p "$parent" | Out-Null
    & $Adb push $source $destination | Out-Null
    $pushed++
}
Write-Output ("pushed={0} missing={1} target={2}" -f $pushed, $missing, $target)
if ($missing) { exit 1 }
exit 0
