# The chunk 18 lifecycle gate: the second quarter of PLAN 4.8.
#
# The rows are only worth writing if they correlate, so this checks the correlation rather than
# the presence: that every sample id in an acquisition or present row resolves to a receive row,
# that the receive rows cover every sample the pipeline applied, and that the four lifecycle
# states are an exhaustive partition over them.
[CmdletBinding()]
param(
    [string]$Player = (Join-Path $PSScriptRoot '../runtime/UnRealDash/Saved/StagedBuilds/Windows/UnRealDash.exe'),
    # Declares a signal nothing binds, which is the only one of PLAN's acquired-not-displayed
    # reasons reachable without forcing the two sides out of step. The rest are chunk 19's.
    [string]$Package = (Join-Path $PSScriptRoot '../tests/fixtures/packages/lifecycle-stage0.udash'),
    [string]$WorkDirectory = (Join-Path $PSScriptRoot '../.tmp/lifecycle-gate'),
    [int]$Seconds = 30
)
$ErrorActionPreference = 'Stop'

$failures = 0
function Fail([string]$message) { $script:failures++; Write-Host "FAIL $message" }

New-Item -ItemType Directory -Force -Path $WorkDirectory | Out-Null
foreach ($path in @($Player, $Package)) {
    if (-not (Test-Path $path)) { Write-Host "Missing: $path. Run tools/gen-chunk18-fixture.py"; exit 2 }
}
$playerRoot = Join-Path (Split-Path $Player -Parent) 'UnRealDash'
$saved = Join-Path $playerRoot 'Saved'
$playerLog = Join-Path $saved 'Logs/UnRealDash.log'
$eventLog = Join-Path $saved 'events/run.jsonl'
$validator = Join-Path $PSScriptRoot 'validate-event-log.py'

Get-Process UnRealDash -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 2
foreach ($stale in @($playerLog, $eventLog)) {
    if (Test-Path $stale) { Remove-Item $stale -Force -ErrorAction SilentlyContinue }
}

Write-Host "a $Seconds second run with one declared signal bound to nothing"
$arguments = @('-windowed', '-ResX=640', '-ResY=360', '-nosplash', '-unattended',
    "-udash=$Package", "-udash-sweep=$Seconds", "-quit-after=$($Seconds + 2)")
$process = Start-Process -FilePath $Player -ArgumentList $arguments -PassThru
if (-not $process.WaitForExit(($Seconds + 90) * 1000)) {
    Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
    Fail 'the run did not exit'
}

$counts = $null
if (Test-Path $playerLog) {
    foreach ($line in (Get-Content $playerLog)) {
        if ($line -match 'DashEventLog .*dropped=(\d+) .*receives=(\d+) acquires=(\d+) submits=(\d+) presents=(\d+) applied=(\d+)') {
            $counts = [pscustomobject]@{
                Dropped = [int64]$Matches[1]; Receives = [int64]$Matches[2]; Acquires = [int64]$Matches[3]
                Submits = [int64]$Matches[4]; Presents = [int64]$Matches[5]; Applied = [int64]$Matches[6]
            }
        }
    }
}
if (-not $counts) {
    Fail 'the player wrote no DashEventLog counts line'
} else {
    Write-Host "  receives $($counts.Receives), acquires $($counts.Acquires), submits $($counts.Submits), presents $($counts.Presents), dropped $($counts.Dropped)"
    if ($counts.Dropped -ne 0) { Fail "$($counts.Dropped) rows were dropped" }
    # Exactly, not within a percent. A dropped receive row is a lost sample, and using the row
    # count as the published count would let the denominator shrink to match the loss.
    if ($counts.Receives -ne $counts.Applied) {
        Fail "$($counts.Receives) receive rows for $($counts.Applied) samples the pipeline applied"
    } else {
        Write-Host "  criterion 2a: $($counts.Receives) receive rows for $($counts.Applied) applied samples, exactly"
    }
    if ($counts.Presents -le 0) { Fail 'no present rows were written' }
}

Write-Host ''
Write-Host 'criteria 1, 2 and 4: correlation and the partition'
$output = & python $validator $eventLog --lifecycle `
    --expect-types frame,sampled,receive,acquire,submit,present --min-seconds ($Seconds - 2) 2>&1
$output | ForEach-Object { Write-Host "  $_" }
if ($LASTEXITCODE -ne 0) { Fail 'the validator rejected the run' }

$text = ($output -join "`n")
# Criterion 3. The unbound signal's samples must land here and nowhere else.
if ($text -notmatch 'bound to no visible component:\s+(\d+)') {
    Fail 'criterion 3: no sample was classified as bound to no visible component'
} elseif ([int]$Matches[1] -lt 100) {
    Fail "criterion 3: only $($Matches[1]) samples were bound to no visible component, expected the whole unbound signal"
} else {
    Write-Host "  criterion 3: $($Matches[1]) samples of the unbound signal, none of them presented"
}
# Criterion 4. At this frame rate every sample is on screen for many frames and must still yield
# one observation each.
if ($text -match 'observations (\d+), (\d+) of them displayed across more than one frame') {
    if ([int]$Matches[2] -eq 0) { Fail 'criterion 4: no sample was displayed across more than one frame, so the case did not run' }
    else { Write-Host "  criterion 4: $($Matches[1]) observations, $($Matches[2]) samples spanning several frames" }
} else {
    Fail 'criterion 4: the classifier reported no observation count'
}

Write-Host ''
Write-Host 'criterion 5: the classifier must be able to fail'
$lines = Get-Content $eventLog

# A sample id in an acquisition row that no receive row accounts for. This is PLAN's own clause
# and it is the one that catches an id assigned in two places.
$orphan = Join-Path $WorkDirectory 'orphan-sample.jsonl'
($lines + '{"type":"acquire","frame":999999,"t_acquire":1,"samples":[999999999]}') | Set-Content $orphan -Encoding utf8
& python $validator $orphan --lifecycle --quiet 2>&1 | Out-Null
if ($LASTEXITCODE -eq 0) { Fail 'the classifier accepted an acquisition row naming a sample with no receive row' }
else { Write-Host '  an acquisition row naming an unknown sample is rejected' }

# Every present row removed. Nothing can be presented, everything acquired must fall to
# acquired-not-displayed, and the four counts must still sum: the partition has to stay
# exhaustive when its happy path is gone.
$nopresent = Join-Path $WorkDirectory 'no-present.jsonl'
($lines | Where-Object { $_ -notmatch '"type":"present"' }) | Set-Content $nopresent -Encoding utf8
$mutated = & python $validator $nopresent --lifecycle 2>&1
$mutatedText = ($mutated -join "`n")
if ($LASTEXITCODE -ne 0) {
    Fail 'removing the present rows broke the classifier rather than moving samples between states'
} elseif ($mutatedText -notmatch 'presented\s+0\b') {
    Fail 'with no present rows something was still classified as presented'
} else {
    $sum = if ($mutatedText -match 'sum\s+(\d+) of (\d+) published') { "$($Matches[1]) of $($Matches[2])" } else { 'unknown' }
    Write-Host "  with every present row removed: presented 0, and the partition still sums, $sum"
}

Write-Host ''
Write-Host "Lifecycle gate: $failures failures"
if ($failures) { exit 1 }
exit 0
