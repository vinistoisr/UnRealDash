# The chunk 17 event log gate: the first quarter of PLAN 4.8.
#
# It checks the three things a log has to get right before anything downstream can read it: that
# the rows are all there, that they arrive at the cadence they claim, and that a kill leaves the
# evidence up to the last second rather than an empty file.
[CmdletBinding()]
param(
    [string]$Player = (Join-Path $PSScriptRoot '../runtime/UnRealDash/Saved/StagedBuilds/Windows/UnRealDash.exe'),
    [string]$Package = (Join-Path $PSScriptRoot '../tests/fixtures/packages/dials-stage0.udash'),
    [string]$WorkDirectory = (Join-Path $PSScriptRoot '../.tmp/event-log-gate'),
    # 60 seconds is PLAN 4.8's figure. Shortening it would make the 1 percent checks looser, not
    # faster to satisfy.
    [int]$Seconds = 60
)
$ErrorActionPreference = 'Stop'

$failures = 0
function Fail([string]$message) { $script:failures++; Write-Host "FAIL $message" }

New-Item -ItemType Directory -Force -Path $WorkDirectory | Out-Null
foreach ($path in @($Player, $Package)) {
    if (-not (Test-Path $path)) { Write-Host "Missing: $path"; exit 2 }
}
$playerRoot = Join-Path (Split-Path $Player -Parent) 'UnRealDash'
$saved = Join-Path $playerRoot 'Saved'
$playerLog = Join-Path $saved 'Logs/UnRealDash.log'
$eventLog = Join-Path $saved 'events/run.jsonl'
$validator = Join-Path $PSScriptRoot 'validate-event-log.py'

function Start-Player([string[]]$Extra, [switch]$KeepEventLogPath) {
    $stalePaths = if ($KeepEventLogPath) { @($playerLog) } else { @($playerLog, $eventLog) }
    foreach ($stale in $stalePaths) {
        if (Test-Path $stale) {
            Remove-Item $stale -Force -ErrorAction SilentlyContinue
            if (Test-Path $stale) {
                Fail 'a previous run is still holding a log file'
                Get-Process UnRealDash -ErrorAction SilentlyContinue | Stop-Process -Force
                Start-Sleep -Seconds 2
                Remove-Item $stale -Force -ErrorAction SilentlyContinue
            }
        }
    }
    $arguments = @('-windowed', '-ResX=640', '-ResY=360', '-nosplash', '-unattended', "-udash=$Package") + $Extra
    return Start-Process -FilePath $Player -ArgumentList $arguments -PassThru
}

# The counts line the player writes at EndPlay. engine_frames is GFrameCounter's own delta, so
# criterion 1 is not the log measured against itself.
function Get-Counts {
    if (-not (Test-Path $playerLog)) { return $null }
    foreach ($line in (Get-Content $playerLog)) {
        if ($line -match 'DashEventLog records=(\d+) frames=(\d+) samples=(\d+) dropped=(\d+) bytes=(\d+) engine_frames=(\d+)') {
            return [pscustomobject]@{
                Records = [int64]$Matches[1]; Frames = [int64]$Matches[2]
                Samples = [int64]$Matches[3]; Dropped = [int64]$Matches[4]
                Bytes = [int64]$Matches[5]; EngineFrames = [int64]$Matches[6]
            }
        }
    }
    return $null
}

Write-Host "criteria 1, 2, 4 and 5: a $Seconds second run"
$process = Start-Player @("-quit-after=$Seconds")
if (-not $process.WaitForExit(($Seconds + 90) * 1000)) {
    Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
    Fail "the run did not exit within $($Seconds + 90) s"
}
$counts = Get-Counts
if (-not $counts) {
    Fail 'the player wrote no DashEventLog counts line'
} else {
    Write-Host "  engine frames $($counts.EngineFrames), frame rows $($counts.Frames), sampled rows $($counts.Samples), dropped $($counts.Dropped)"
    if ($counts.Dropped -ne 0) { Fail "$($counts.Dropped) rows were dropped" }
    if ($counts.EngineFrames -le 0) {
        Fail 'the engine reported no frames'
    } else {
        # Criterion 1. Against GFrameCounter, not against the row count.
        $off = [Math]::Abs($counts.Frames - $counts.EngineFrames) / [double]$counts.EngineFrames
        Write-Host ("  criterion 1: frame rows are {0:P2} off the engine's frame count, limit 1.00%" -f $off)
        if ($off -gt 0.01) { Fail 'the frame row count is more than 1 percent off the engine frame count' }
    }
}

# Criteria 2, 4 and 5 all come out of the validator: the declared schema, the timestamp columns,
# and the sampled cadence measured from the rows' own t column rather than from how many there are.
$output = & python $validator $eventLog --expect-types frame,sampled --cadence sampled=1.0 --min-seconds ($Seconds - 2) 2>&1
$output | ForEach-Object { Write-Host "  $_" }
if ($LASTEXITCODE -ne 0) { Fail 'the validator rejected the 60 second log' }

Write-Host ''
Write-Host 'criterion 4 again, the other direction: a row of an undeclared type must be rejected'
# A validator that only ever passes is not a check. This is the mutation that proves it bites.
$mutated = Join-Path $WorkDirectory 'undeclared.jsonl'
$lines = Get-Content $eventLog
$lines + '{"type":"invented","t":1}' | Set-Content $mutated -Encoding utf8
& python $validator $mutated --quiet 2>&1 | Out-Null
if ($LASTEXITCODE -eq 0) { Fail 'the validator accepted a row of an undeclared type' }
else { Write-Host '  rejected' }

$renamed = Join-Path $WorkDirectory 'renamed-column.jsonl'
# Only the data rows. Renaming it in the schema record too would leave the file self-consistent,
# which is what the first attempt did and why it passed: the mutation has to make a row disagree
# with what its own log declared.
($lines | ForEach-Object {
    if ($_ -match '"type":"schema"') { $_ } else { $_ -replace '"resident_bytes"', '"resident"' }
}) | Set-Content $renamed -Encoding utf8
& python $validator $renamed --quiet 2>&1 | Out-Null
if ($LASTEXITCODE -eq 0) { Fail 'the validator accepted a row whose columns do not match the schema' }
else { Write-Host '  and a renamed column is rejected too' }

Write-Host ''
Write-Host 'criterion 3: a kill at 30 seconds leaves a readable log'
$process = Start-Player @('-quit-after=300')
# Timed from when the log appears, not from Start-Process. The player spends about a second and a
# half booting before it opens anything, and counting that as logged time is how the 29 second
# clause fails by four tenths of a second for no reason worth chasing.
$appeared = $false
for ($waited = 0; $waited -lt 60 -and -not $appeared; ++$waited) {
    Start-Sleep -Seconds 1
    $appeared = Test-Path $eventLog
}
if (-not $appeared) { Fail 'the run never created an event log to kill' }
Start-Sleep -Seconds 30
# By name, not by id. The player spawns a child, Stop-Process on the parent alone leaves it
# holding the log files, and the next case then fails for a reason that is not about it.
Get-Process UnRealDash -ErrorAction SilentlyContinue | Stop-Process -Force
for ($waited = 0; $waited -lt 30; ++$waited) {
    if (-not (Get-Process UnRealDash -ErrorAction SilentlyContinue)) { break }
    Start-Sleep -Seconds 1
}
if (-not (Test-Path $eventLog)) {
    Fail 'the killed run left no event log at all'
} else {
    # 29 of 30 seconds, which is 4.8's clause. The usual loss is the unflushed second rather than
    # a torn line, and the validator tolerates at most one trailing partial line either way.
    $output = & python $validator $eventLog --expect-types frame,sampled --min-seconds 29 2>&1
    $output | ForEach-Object { Write-Host "  $_" }
    if ($LASTEXITCODE -ne 0) { Fail 'the killed run did not leave 29 seconds of readable rows' }
}

Write-Host ''
Write-Host 'criterion 7: a log that cannot be opened does not stop the run'
# A directory where the file should be. The open fails, and a dashboard that refused to start
# because it could not write telemetry about itself would be worse than one that starts without it.
$setup = $false
for ($attempt = 0; $attempt -lt 20 -and -not $setup; ++$attempt) {
    if (Test-Path $eventLog) { Remove-Item $eventLog -Recurse -Force -ErrorAction SilentlyContinue }
    if (-not (Test-Path $eventLog)) {
        New-Item -ItemType Directory -Force -Path $eventLog | Out-Null
        # The setup is the case. If the path is not a directory, the run below opens its log
        # normally and the check silently becomes a no-op, which is how this first passed for the
        # wrong reason.
        $setup = (Test-Path $eventLog -PathType Container)
    }
    if (-not $setup) { Start-Sleep -Milliseconds 500 }
}
if (-not $setup) { Fail 'could not put a directory where the event log goes, so the case did not run' }
$shot = Join-Path $WorkDirectory 'no-log.png'
if (Test-Path $shot) { Remove-Item $shot -Force }
# -KeepEventLogPath, or the launcher's own stale-log cleanup deletes the directory that is the
# whole point of this case and the run opens its log normally.
$process = Start-Player -Extra @("-screenshot=$shot", '-quit-after=8') -KeepEventLogPath
if (-not $process.WaitForExit(90000)) { Stop-Process -Id $process.Id -Force; Fail 'the run with no log did not exit' }
elseif ($process.ExitCode -ne 0) { Fail "the run with no log exited $($process.ExitCode)" }
elseif (-not (Test-Path $shot)) { Fail 'the run with no log rendered nothing' }
else {
    $said = Get-Content $playerLog | Select-String 'DashEventLog unavailable'
    if (-not $said) { Fail 'the run with no log did not say why there is no log' }
    else { Write-Host "  ran and rendered, and said: $(($said | Select-Object -First 1).Line -replace '.*DashEventLog', 'DashEventLog')" }
}
Remove-Item $eventLog -Recurse -Force -ErrorAction SilentlyContinue

Write-Host ''
Write-Host "Event log gate: $failures failures"
if ($failures) { exit 1 }
exit 0
