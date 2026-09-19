# The PLAN 4.5 screenshot gate: criteria 2, 3 and 4 of docs/build/chunk-11-primitives.md.
#
# It launches the packaged player once per capture with the frozen render settings, compares each
# frame through scripts/compare-capture.py, and exits non-zero if any criterion fails.
#
# This gate does not run in CI and must not be made to. GPU, driver and font rasterisation all move
# the comparator's numbers, and the hosted runners have no GPU. A reference captured on a different
# machine is reported as not run, never as a pass and never as a failure.
[CmdletBinding()]
param(
    # The packaged player. Defaults to the Windows staged build the packaging scripts produce.
    [string]$Player = (Join-Path $PSScriptRoot '../runtime/UnRealDash/Saved/StagedBuilds/Windows/UnRealDash.exe'),
    [string]$Package = (Join-Path $PSScriptRoot '../tests/fixtures/packages/all-primitives-stage0.udash'),
    [string]$ReferenceDirectory = (Join-Path $PSScriptRoot '../tests/fixtures/references'),
    [string]$WorkDirectory = (Join-Path $PSScriptRoot '../.tmp/capture-gate'),
    # Writes the captures into the reference directory instead of comparing against it. Run this
    # once on a machine, look at every frame, and commit them with capture-environment.json.
    [switch]$Bless
)
$ErrorActionPreference = 'Stop'

# The capture conditions are part of the gate, not a person's memory. Without them a frame is not
# deterministic: temporal antialiasing, bloom, auto exposure and motion blur all move between runs
# on the same machine, and the comparator would report a change nobody made.
$exec = 'r.PostProcessAAQuality 0, r.DefaultFeature.AntiAliasing 0, r.DefaultFeature.Bloom 0, ' +
        'r.DefaultFeature.AutoExposure 0, r.DefaultFeature.MotionBlur 0, r.ScreenPercentage 100, ' +
        'r.Tonemapper.Sharpen 0'

# The fixture geometry these thresholds are proportioned against. A 1280x720 frame is 921,600
# pixels; the gauge fill is 320x180 (57,600) and the status band is 320x48 (15,360). If
# tools/gen-chunk11-fixture.py changes either, redo this arithmetic before trusting a green run.
$frame = 1280 * 720
$passMoved = 0.001        # 922 pixels
$distinctMoved = 0.005    # 4,608 pixels, criterion 4
$mutationMoved = 0.01     # 9,216 pixels, criterion 3

$failures = 0
function Fail([string]$message) { $script:failures++; Write-Host "FAIL $message" }

# The running machine, passed to every comparison. A reference captured on different hardware is
# reported as not run, and the comparator treats a value this script fails to supply as a mismatch
# rather than as agreement, so an environment check cannot be satisfied by silence.
$adapter = Get-CimInstance Win32_VideoController | Sort-Object -Property AdapterRAM -Descending | Select-Object -First 1
$engineRoot = (Get-Content (Join-Path $PSScriptRoot 'pins.json') -Raw | ConvertFrom-Json).engine.root
$build = Get-Content (Join-Path $engineRoot 'Engine/Build/Build.version') -Raw | ConvertFrom-Json
$engineVersion = "$($build.MajorVersion).$($build.MinorVersion).$($build.PatchVersion)"
$environment = [ordered]@{
    gpu        = $adapter.Name
    driver     = $adapter.DriverVersion
    # The engine's own Build.version, not a pin: pins.json records where the engine is, and a
    # capture is only comparable against one taken by the same build. Doctor already checks that
    # this is 5.8.2, so this records it rather than re-asserting it.
    engine     = $engineVersion
    resolution = '1280x720'
}
# Every key here is sent to every comparison, and the comparator treats a key it was not given as a
# mismatch. So adding a key to capture-environment.json cannot quietly go unchecked.
$environmentArguments = @($environment.GetEnumerator() | ForEach-Object { '--env'; "$($_.Key)=$($_.Value)" })
Write-Host ('Capture environment: ' + (($environment.GetEnumerator() | ForEach-Object { "$($_.Key)=$($_.Value)" }) -join ' '))

New-Item -ItemType Directory -Force -Path $WorkDirectory | Out-Null
if (-not (Test-Path $Player)) { Write-Host "No packaged player at $Player"; exit 2 }
if (-not (Test-Path $Package)) { Write-Host "No fixture package at $Package. Run tools/gen-chunk11-fixture.py"; exit 2 }

function Invoke-Capture([string]$PackagePath, [string]$State, [string]$Output) {
    if (Test-Path $Output) { Remove-Item $Output -Force }
    $arguments = @(
        '-windowed', '-ResX=1280', '-ResY=720', '-nosplash', '-unattended',
        "-udash=$PackagePath", "-udash-state=$State", "-screenshot=$Output",
        "-ExecCmds=$exec"
    )
    $process = Start-Process -FilePath $Player -ArgumentList $arguments -PassThru -Wait
    if ($process.ExitCode -ne 0) { Fail "capture $State exited $($process.ExitCode)" }
    if (-not (Test-Path $Output)) { Fail "capture $State produced no file at $Output"; return $false }
    return $true
}

function Compare-Frames([string]$Reference, [string]$Candidate) {
    $output = & python (Join-Path $PSScriptRoot 'compare-capture.py') $Reference $Candidate `
        @environmentArguments 2>&1
    return [pscustomobject]@{ Code = $LASTEXITCODE; Text = ($output -join "`n") }
}

# Every comparison reports worst and moved, pass or fail, so a green run still shows its numbers.
function Get-Moved([string]$Text) {
    if ($Text -match 'moved=([0-9.]+)') { return [double]$Matches[1] }
    return [double]::NaN
}

$states = @('valid', 'age_unknown', 'stale')
$captures = @{}
foreach ($state in $states) {
    $path = Join-Path $WorkDirectory "all-primitives-stage0.$state.png"
    if (Invoke-Capture $Package $state $path) { $captures[$state] = $path }
}

if ($Bless) {
    New-Item -ItemType Directory -Force -Path $ReferenceDirectory | Out-Null
    foreach ($state in $states) {
        if ($captures.ContainsKey($state)) {
            Copy-Item $captures[$state] (Join-Path $ReferenceDirectory "all-primitives-stage0.$state.png") -Force
        }
    }
    # Written here rather than by hand, so the recorded environment is the one that produced these
    # frames and not what somebody believed it was.
    $environment | ConvertTo-Json | Set-Content (Join-Path $ReferenceDirectory 'capture-environment.json') -Encoding utf8
    Write-Host "Blessed $($captures.Count) references into $ReferenceDirectory."
    Write-Host 'Look at every one before committing them.'
    if ($failures) { exit 1 }
    exit 0
}

# Criterion 2: the valid capture matches the checked-in reference.
$reference = Join-Path $ReferenceDirectory 'all-primitives-stage0.valid.png'
if (-not (Test-Path $reference)) {
    Write-Host "NOT RUN: no reference at $reference. Capture one with -Bless on this machine."
    exit 3
}
$result = Compare-Frames $reference $captures['valid']
Write-Host "criterion 2 (valid matches reference): $($result.Text)"
if ($result.Code -eq 3) { Write-Host 'NOT RUN: reference was captured on a different machine.'; exit 3 }
if ($result.Code -ne 0) { Fail 'criterion 2: valid capture does not match the reference' }

# Criterion 4: the three states are mutually distinguishable. Reported as three pair numbers rather
# than a verdict, because a pair that only just clears the threshold is worth seeing.
foreach ($pair in @(@('valid', 'age_unknown'), @('valid', 'stale'), @('age_unknown', 'stale'))) {
    $result = Compare-Frames $captures[$pair[0]] $captures[$pair[1]]
    $moved = Get-Moved $result.Text
    $pixels = [int]($moved * $frame)
    Write-Host ("criterion 4 pair {0}/{1}: moved={2:F6} ({3} pixels, needs over {4})" -f `
        $pair[0], $pair[1], $moved, $pixels, $distinctMoved)
    if (-not ($moved -gt $distinctMoved)) { Fail "criterion 4: $($pair[0]) and $($pair[1]) are not distinguishable" }
}

# Criterion 3: the named mutation must break the gate, and break it by a wide margin. A mutation
# that only just crosses the line proves the comparator is twitchy, not that the gate works.
$mutated = Join-Path $WorkDirectory 'mutated'
$mutatedPackage = Join-Path $WorkDirectory 'mutated.udash'
python (Join-Path $PSScriptRoot 'mutate-chunk11-fixture.py') $mutated $mutatedPackage
if ($LASTEXITCODE -ne 0) { Fail 'criterion 3: could not build the mutated package'; exit 1 }
$mutatedShot = Join-Path $WorkDirectory 'mutated.valid.png'
if (Invoke-Capture $mutatedPackage 'valid' $mutatedShot) {
    $result = Compare-Frames $reference $mutatedShot
    $moved = Get-Moved $result.Text
    $pixels = [int]($moved * $frame)
    Write-Host ("criterion 3 (mutation): {0}" -f $result.Text)
    Write-Host ("criterion 3: moved={0:F6} ({1} pixels, needs over {2})" -f $moved, $pixels, $mutationMoved)
    if ($result.Code -eq 0) { Fail 'criterion 3: the mutation PASSED the comparator, so the gate cannot fail' }
    if (-not ($moved -gt $mutationMoved)) { Fail 'criterion 3: the mutation moved too few pixels to prove the gate works' }
}

# And the fixture still passes afterwards, so the mutation was the only difference. The fixture is
# regenerated rather than edited in place, so there is nothing to restore.
python (Join-Path $PSScriptRoot '../tools/gen-chunk11-fixture.py') | Out-Null
$result = Compare-Frames $reference $captures['valid']
Write-Host "criterion 3 (restored): $($result.Text)"
if ($result.Code -ne 0) { Fail 'criterion 3: the fixture does not pass again after the mutation' }

Write-Host ''
Write-Host "Capture gate: $failures failures"
if ($failures) { exit 1 }
exit 0
