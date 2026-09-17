# Device half of the PLAN 4.4 gate, run on the desk device.
#
# The desktop half runs as a commandlet, which needs the editor and so cannot run on Android.
# Here the packaged player is launched once per fixture with -udash=, and the verdict is read back
# from logcat. ADashPackageHUD emits one "DashVerdict accepted=.. code=.. pointer=.. path=.." line
# per run precisely so this is machine readable: an accepted package otherwise logs nothing, which
# would make success indistinguishable from an early crash.
#
# Expected outcomes come from tests/fixtures/packages/cases.json, never from a hardcoded list.
# An empty code in that file means the case must be ACCEPTED.
[CmdletBinding()]
param(
    [string]$Adb = "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe",
    [string]$Package = 'com.unrealdash.player',
    [int]$LaunchSeconds = 7,
    [switch]$ArchiveOnly,
    [int]$Limit = 0,
    # One launch instead of one per fixture. The player loads every fixture and reports a verdict
    # for each; this script still decides what was expected, so the game module carries no test
    # expectations. Roughly 2 minutes of device time instead of 98.
    [switch]$Batch
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$cases = Get-Content (Join-Path $root 'tests/fixtures/packages/cases.json') -Raw | ConvertFrom-Json

$base = "/storage/emulated/0/Android/data/$Package/files/UnrealGame/UnRealDash"
$fixtures = "$base/UnRealDash/Saved/Fixtures"

function Invoke-Case([string]$DevicePath, [string]$Label) {
    # UECommandLine.txt REPLACES the command line, so it carries -project= itself. The map is
    # overridden to the package game mode with ?game=, because the cooked map defaults to the
    # smoke spike's mode.
    $cmd = "-project=../../../UnRealDash/UnRealDash.uproject /Game/Smoke/L_Smoke?game=/Script/UnRealDash.DashPackageGameMode -udash=$DevicePath"
    $tmp = Join-Path $env:TEMP 'UECommandLine.txt'
    [IO.File]::WriteAllText($tmp, $cmd)
    & $Adb push $tmp "$base/UECommandLine.txt" | Out-Null
    & $Adb shell am force-stop $Package | Out-Null
    & $Adb logcat -c | Out-Null
    & $Adb shell monkey -p $Package -c android.intent.category.LAUNCHER 1 2>&1 | Out-Null
    Start-Sleep -Seconds $LaunchSeconds
    $log = (& $Adb logcat -d -s UE:*) -join "`n"
    & $Adb shell am force-stop $Package | Out-Null
    $m = [regex]::Match($log, 'DashVerdict accepted=(\S+) code=(\S*) pointer=(\S*)')
    if (-not $m.Success) { return @{ Label = $Label; Ran = $false } }
    @{ Label = $Label; Ran = $true; Accepted = ($m.Groups[1].Value -eq 'true'); Code = $m.Groups[2].Value; Pointer = $m.Groups[3].Value }
}

if ($Batch) {
    # One launch per profile. The player reports a verdict for every fixture it finds, including
    # directory forms of the archive-only cases; this script decides what was expected, so the game
    # module carries no test expectations. Two launches, about two minutes, instead of 98.
    $tables = @{}
    foreach ($prof in 'mobile', 'desktop') {
        $cmd = "-project=../../../UnRealDash/UnRealDash.uproject /Game/Smoke/L_Smoke?game=/Script/UnRealDash.DashPackageGameMode -udash-batch=$fixtures -udash-profile=$prof"
        $tmp = Join-Path $env:TEMP 'UECommandLine.txt'
        [IO.File]::WriteAllText($tmp, $cmd)
        & $Adb push $tmp "$base/UECommandLine.txt" | Out-Null
        & $Adb shell am force-stop $Package | Out-Null
        & $Adb logcat -c | Out-Null
        & $Adb shell monkey -p $Package -c android.intent.category.LAUNCHER 1 2>&1 | Out-Null
        $log = ''
        for ($i = 0; $i -lt 48; $i++) {
            Start-Sleep -Seconds 5
            $log = (& $Adb logcat -d -s UE:*) -join "`n"
            if ($log -match 'DashBatch total=') { break }
        }
        & $Adb shell am force-stop $Package | Out-Null
        $t = @{}
        foreach ($m in [regex]::Matches($log, 'DashVerdict accepted=(\S+) code=(\S*) pointer=(\S*) path=(\S+)')) {
            $leaf = ($m.Groups[4].Value.TrimEnd('/') -split '/')[-1]
            $t[$leaf] = @{ Accepted = ($m.Groups[1].Value -eq 'true'); Code = $m.Groups[2].Value; Pointer = $m.Groups[3].Value }
        }
        $tables[$prof] = $t
        Write-Output ("batch profile={0} verdicts={1}" -f $prof, $t.Count)
    }

    $results = @()
    foreach ($c in $cases) {
        $expectAccept = [string]::IsNullOrEmpty($c.code)
        # A case names its profile when the outcome depends on the texture budget; otherwise the
        # device default, which is mobile on Android, is the meaningful one.
        $prof = if ($c.profile) { $c.profile } else { 'mobile' }
        $table = $tables[$prof]
        $forms = @(@{ Kind = 'archive'; Leaf = "$($c.name).udash" })
        if (-not $c.archive_only) { $forms += @{ Kind = 'directory'; Leaf = $c.name } }
        foreach ($f in $forms) {
            $v = $table[$f.Leaf]
            $r = if ($v) { @{ Label = "$($c.name) [$($f.Kind)]"; Ran = $true; Accepted = $v.Accepted; Code = $v.Code; Pointer = $v.Pointer } }
                 else     { @{ Label = "$($c.name) [$($f.Kind)]"; Ran = $false } }
            $r.Ok = $r.Ran -and ($r.Accepted -eq $expectAccept) -and ($expectAccept -or $r.Code -eq $c.code)
            $results += [pscustomobject]$r
            if (-not $r.Ok) { Write-Output ("{0,-46} MISMATCH profile={1} expected accept={2} code={3}; got accept={4} code={5} ran={6}" -f $r.Label, $prof, $expectAccept, $c.code, $r.Accepted, $r.Code, $r.Ran) }
        }
    }
    Write-Output ("evaluated {0} expected forms of {1} cases" -f $results.Count, $cases.Count)
} else {

$results = @()
$n = 0
foreach ($c in $cases) {
    if ($Limit -gt 0 -and $n -ge $Limit) { break }
    $n++
    $expectAccept = [string]::IsNullOrEmpty($c.code)
    $forms = @(@{ Kind = 'archive'; Path = "$fixtures/$($c.name).udash" })
    if (-not $ArchiveOnly -and -not $c.archive_only) { $forms += @{ Kind = 'directory'; Path = "$fixtures/$($c.name)" } }
    foreach ($f in $forms) {
        $r = Invoke-Case $f.Path "$($c.name) [$($f.Kind)]"
        $r.ExpectAccept = $expectAccept
        $r.ExpectCode = $c.code
        $r.Ok = $r.Ran -and ($r.Accepted -eq $expectAccept) -and ($expectAccept -or $r.Code -eq $c.code)
        $results += [pscustomobject]$r
        Write-Output ("{0,-46} {1}" -f $r.Label, $(if ($r.Ok) { 'ok' } else { "MISMATCH expected accept=$expectAccept code=$($c.code); got accept=$($r.Accepted) code=$($r.Code) ran=$($r.Ran)" }))
    }
}

}

# Archive and directory forms of the same case must agree, which is the parity half of the gate.
$parityFail = 0
foreach ($g in $results | Group-Object { ($_.Label -split ' \[')[0] }) {
    if ($g.Count -lt 2) { continue }
    $a = $g.Group | Where-Object { $_.Label -like '*[archive]*' }
    $d = $g.Group | Where-Object { $_.Label -like '*[directory]*' }
    if ($a -and $d -and (($a.Accepted -ne $d.Accepted) -or ($a.Code -ne $d.Code) -or ($a.Pointer -ne $d.Pointer))) {
        $parityFail++
        Write-Output ("PARITY  {0}: archive accept={1} code={2} pointer={3} | directory accept={4} code={5} pointer={6}" -f $g.Name, $a.Accepted, $a.Code, $a.Pointer, $d.Accepted, $d.Code, $d.Pointer)
    }
}

$bad = @($results | Where-Object { -not $_.Ok }).Count
Write-Output ""
Write-Output ("device gate runs={0} mismatches={1} parity_failures={2}" -f $results.Count, $bad, $parityFail)
if ($bad -or $parityFail) { exit 1 }
exit 0
