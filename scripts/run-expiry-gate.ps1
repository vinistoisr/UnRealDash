# The chunk 19 expiry and forcing gate: the third quarter of PLAN 4.8.
#
# Five of its cases exist only because they can be forced. PLAN's lifecycle and latency clauses
# describe runs in which the acquisition side and the presentation side are deliberately out of
# step, and a player that always behaves cannot produce them.
[CmdletBinding()]
param(
    [string]$Player = (Join-Path $PSScriptRoot '../runtime/UnRealDash/Saved/StagedBuilds/Windows/UnRealDash.exe'),
    [string]$Package = (Join-Path $PSScriptRoot '../tests/fixtures/packages/lifecycle-stage0.udash'),
    [string]$WorkDirectory = (Join-Path $PSScriptRoot '../.tmp/expiry-gate')
)
$ErrorActionPreference = 'Stop'

$failures = 0
function Fail([string]$message) { $script:failures++; Write-Host "FAIL $message" }

New-Item -ItemType Directory -Force -Path $WorkDirectory | Out-Null
foreach ($path in @($Player, $Package)) {
    if (-not (Test-Path $path)) { Write-Host "Missing: $path. Run tools/gen-chunk18-fixture.py"; exit 2 }
}
$saved = Join-Path (Split-Path $Player -Parent) 'UnRealDash/Saved'
$eventLog = Join-Path $saved 'events/run.jsonl'
$validator = Join-Path $PSScriptRoot 'validate-event-log.py'

# Every case is one run plus one classifier pass, so the driver is worth having once.
function Invoke-Case([string[]]$Extra, [string]$Label) {
    Get-Process UnRealDash -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Seconds 2
    if (Test-Path $eventLog) { Remove-Item $eventLog -Force -ErrorAction SilentlyContinue }
    $base = @('-windowed', '-ResX=640', '-ResY=360', '-nosplash', '-unattended', "-udash=$Package")
    $process = Start-Process -FilePath $Player -ArgumentList ($base + $Extra) -PassThru
    if (-not $process.WaitForExit(180000)) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        Fail "$Label did not exit"
    }
    Get-Process UnRealDash -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Seconds 1
    if (-not (Test-Path $eventLog)) { Fail "$Label produced no event log"; return $null }
    $output = & python $validator $eventLog --lifecycle 2>&1
    $ok = $LASTEXITCODE -eq 0
    Write-Host "$Label"
    ($output | Select-String 'presented|acquired-not|sum |no submit|bound to no|submitted,|armed |fired |cancelled,|unresolved|displayed firings|fired-not|expiry-to-present') |
        ForEach-Object { Write-Host "  $($_.Line.Trim())" }
    if (-not $ok) { Fail "${Label}: the validator rejected the log" }
    return ($output -join "`n")
}

function Get-Number([string]$Text, [string]$Pattern) {
    if ($Text -match $Pattern) { return [int]$Matches[1] }
    return -1
}

# A sweep that ends well before the run does, so every deadline really passes and the screen
# really shows the stale presentation. Criteria 1, 5 and 6.
$text = Invoke-Case @('-udash-sweep=3', '-quit-after=12') 'criteria 1, 5 and 6: a source that stops'
if ($text) {
    $armed = Get-Number $text 'armed\s+(\d+)'
    $fired = Get-Number $text 'fired\s+(\d+)'
    $shown = Get-Number $text 'displayed firings\s+(\d+)'
    if ($armed -le 0) { Fail 'no expiry was ever armed' }
    if ($fired -le 0) { Fail 'the source stopped and no expiry fired' }
    if ($shown -le 0) { Fail 'an expiry fired and no frame was recorded displaying it' }
    if ($text -notmatch 'expiry-to-present\s+median') { Fail 'no expiry-to-present latency was computed' }
}

# Criterion 2. The sweep outlives the run, so deadlines are still armed at a clean exit.
$text = Invoke-Case @('-udash-sweep=60', '-quit-after=8') 'criterion 2: a clean exit with deadlines still armed'
if ($text) {
    if ($text -notmatch 'cancelled, run_end\s+([1-9]\d*)') { Fail 'a clean exit wrote no run_end cancellation' }
    if ((Get-Number $text 'unresolved\s+(\d+)') -ne 0) { Fail 'a clean exit left an expiry unresolved' }
}

# Criterion 3. Samples arrive faster than the deadline, so almost every arming is re-armed away.
if ($text -and $text -notmatch 'cancelled, re-armed\s+([1-9]\d*)') {
    Fail 'no expiry was cancelled by a later sample re-arming it'
}

# Criterion 4. A hard exit with no EndPlay, so nothing writes the run_end rows.
$text = Invoke-Case @('-udash-sweep=60', '-udash-kill-at=6', '-quit-after=90') 'criterion 4: a hard kill mid-flight'
if ($text) {
    if ((Get-Number $text 'unresolved\s+(\d+)') -le 0) { Fail 'a hard kill left no unresolved expiry' }
    if ($text -match 'cancelled, run_end\s+([1-9]\d*)') { Fail 'a hard kill wrote run_end rows, so it was not hard' }
}

# Criterion 7, the four forced lifecycle branches. Every one of them must still partition.
$text = Invoke-Case @('-udash-sweep=10', '-quit-after=8', '-udash-suppress-submit') 'criterion 7a: submission suppressed'
if ($text) {
    if ((Get-Number $text 'presented\s+(\d+)') -ne 0) { Fail 'a frame with no submit still presented samples' }
    if ($text -notmatch 'no submit row: ([1-9]\d*)') { Fail 'no sample was attributed to a missing submit row' }
}

$text = Invoke-Case @('-udash-sweep=10', '-quit-after=8', '-udash-hold-frame-ms=30') 'criterion 7b: frames held between acquire and apply'
if ($text) {
    if ((Get-Number $text 'presented\s+(\d+)') -le 0) { Fail 'holding the frames stopped presentation entirely' }
}

Write-Host ''
Write-Host "Expiry gate: $failures failures"
if ($failures) { exit 1 }
exit 0
