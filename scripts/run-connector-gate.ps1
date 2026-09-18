# PLAN 4.9's connector gate: live values over a real socket, stale when the relay dies, and a
# reconnect with no player restart.
#
# Criterion 6 is a sequence, not a state, so no capture can show it: the relay is killed, every
# mapped signal goes stale, the relay comes back, and the connector reconnects. This orchestrates
# that sequence against one running player and reads the health log it emits once a second.
#
# Like the other capture gates this does not run in CI. It needs a GPU to render and it binds a
# loopback port.
[CmdletBinding()]
param(
    [string]$Player = (Join-Path $PSScriptRoot '../runtime/UnRealDash/Saved/StagedBuilds/Windows/UnRealDash.exe'),
    [string]$Package = (Join-Path $PSScriptRoot '../tests/fixtures/packages/connector-stage0.udash'),
    [string]$Relay = (Join-Path $PSScriptRoot 'mock-relay.py'),
    [int]$Port = 35000,
    [string]$WorkDirectory = (Join-Path $PSScriptRoot '../.tmp/connector-gate')
)
$ErrorActionPreference = 'Stop'

# The sequence, in seconds from the player starting. Chosen against the backoff schedule of 500,
# 1000, 2000, 4000 then 5000: the relay is away for seven seconds, which is long enough for three
# or four attempts and for every signal to pass its 500 ms deadline, and it returns while the
# interval is still short enough that the gate does not spend a minute waiting.
$killAt = 5
$restartAt = 12
$runFor = 20

$failures = 0
function Fail([string]$message) { $script:failures++; Write-Host "FAIL $message" }

New-Item -ItemType Directory -Force -Path $WorkDirectory | Out-Null
foreach ($path in @($Player, $Package, $Relay)) {
    if (-not (Test-Path $path)) { Write-Host "Missing: $path"; exit 2 }
}
$logDirectory = Join-Path (Split-Path $Player -Parent) 'UnRealDash/Saved/Logs'
$log = Join-Path $logDirectory 'UnRealDash.log'

$relayArguments = @($Relay, '--port', "$Port", '--period', '8', '--rate', '50')

# The port has to be free before anything starts. A relay that cannot bind exits immediately, and
# the only symptom was a null process id at the kill several seconds later, which says nothing
# about the cause. This is the message that does.
function Test-PortBusy([int]$Number) {
    return [bool](Get-NetTCPConnection -LocalPort $Number -State Listen -ErrorAction SilentlyContinue)
}
if (Test-PortBusy $Port) {
    Write-Host "Port $Port is already in use, most likely by a relay left over from an earlier run."
    Write-Host 'Stop it before running this gate:  Get-Process python | Stop-Process -Force'
    exit 2
}

# The pid is kept rather than the Process object, which can report a null Id once the process has
# gone.
function Start-Relay([string[]]$Arguments, [int]$Number) {
    $process = Start-Process -FilePath python -ArgumentList $Arguments -PassThru -WindowStyle Hidden
    for ($i = 0; $i -lt 50; $i++) {
        Start-Sleep -Milliseconds 100
        if (Test-PortBusy $Number) { return $process.Id }
    }
    return 0
}

$relayId = 0
$playerProcess = $null
try {
    Write-Host "starting the relay on 127.0.0.1:$Port"
    $relayId = Start-Relay $relayArguments $Port
    if (-not $relayId) { throw 'The relay never started listening' }
    Write-Host "  relay pid $relayId, listening"

    $arguments = @('-windowed', '-ResX=1280', '-ResY=720', '-nosplash', '-unattended',
        "-udash=$Package", '-connector=tcp', '-connector-host=127.0.0.1', "-connector-port=$Port",
        "-udash-quit-after=$runFor")
    # Not $player: PowerShell variable names are case insensitive, so that is the same variable as the
    # [string]$Player parameter, and assigning a Process to it coerces it straight back to a string.
    # The symptom was WaitForExit failing on a String twenty seconds later.
    $playerProcess = Start-Process -FilePath $Player -ArgumentList $arguments -PassThru

    Start-Sleep -Seconds $killAt
    Write-Host "killing the relay at ${killAt}s"
    Stop-Process -Id $relayId -Force -ErrorAction SilentlyContinue

    Start-Sleep -Seconds ($restartAt - $killAt)
    Write-Host "restarting the relay at ${restartAt}s"
    $relayId = Start-Relay $relayArguments $Port
    if (-not $relayId) { Fail 'the relay did not come back, so criterion 6 cannot be judged' }

    $playerProcess.WaitForExit()
    if ($relayId) { Stop-Process -Id $relayId -Force -ErrorAction SilentlyContinue }
    if ($playerProcess.ExitCode -ne 0) { Fail "the player exited $($playerProcess.ExitCode)" }
}
finally {
    # Always, even on a failure part way through. A run that died leaving a listener behind is why
    # the port check above exists, and leaving one behind again would just move the problem.
    if ($relayId) { Stop-Process -Id $relayId -Force -ErrorAction SilentlyContinue }
    if ($playerProcess -and -not $playerProcess.HasExited) {
        Stop-Process -Id $playerProcess.Id -Force -ErrorAction SilentlyContinue
    }
}

if (-not (Test-Path $log)) { Write-Host "No player log at $log"; exit 2 }

# One row per health line: the whole run, in order.
$rows = @()
foreach ($line in Get-Content $log) {
    if ($line -match 'DashHealth t=([0-9.]+) connected=(\w+) generation=(\d+) reconnects=(\d+) bytes=(\d+) backoff_ms=(\d+) attempts=(\d+)') {
        $rows += [pscustomobject]@{
            Time = [double]$Matches[1]; Connected = ($Matches[2] -eq 'true')
            Generation = [uint64]$Matches[3]; Reconnects = [uint64]$Matches[4]
            Bytes = [uint64]$Matches[5]; Backoff = [uint64]$Matches[6]; Attempts = [uint64]$Matches[7]
        }
    }
}
if ($rows.Count -lt 5) { Fail "the player logged only $($rows.Count) health lines"; }

Write-Host ''
Write-Host 'health over the run:'
foreach ($row in $rows) {
    Write-Host ("  t={0,5:F1} connected={1,-5} generation={2} reconnects={3} bytes={4,-6} backoff_ms={5,-4} attempts={6}" -f `
        $row.Time, $row.Connected, $row.Generation, $row.Reconnects, $row.Bytes, $row.Backoff, $row.Attempts)
}
Write-Host ''

# Criterion 4: live values over the socket. Bytes arriving is the measurement; the live gate
# already proves that arriving bytes move a gauge.
$connectedEarly = @($rows | Where-Object { $_.Time -lt $killAt -and $_.Connected })
if (-not $connectedEarly) { Fail 'criterion 4: never connected to the relay' }
elseif (($connectedEarly | Select-Object -Last 1).Bytes -eq 0) { Fail 'criterion 4: connected but received no bytes' }
else { Write-Host "criterion 4: connected and received $(($connectedEarly | Select-Object -Last 1).Bytes) bytes before the kill" }

# Criterion 5: the relay dies and the connector notices. Every mapped signal going stale follows
# from the registry's own deadlines, which the live gate measures on screen; what this adds is that
# a killed socket is reported as disconnected rather than as quiet.
$down = @($rows | Where-Object { $_.Time -gt $killAt -and $_.Time -lt $restartAt -and -not $_.Connected })
if (-not $down) { Fail 'criterion 5: the connector did not notice the relay dying' }
else {
    $ceiling = ($down | Measure-Object -Property Backoff -Maximum).Maximum
    Write-Host "criterion 5: disconnected within $([int]($down[0].Time - $killAt))s, backoff reached ${ceiling}ms over $($down.Count) samples"
    if ($ceiling -lt 1000) { Fail 'criterion 5: the backoff never grew, so it was not retrying on a schedule' }
}

# Criterion 6: it comes back, with no player restart, and the generation advances so a sample from
# the old connection cannot be mistaken for a new one.
$back = @($rows | Where-Object { $_.Time -gt $restartAt -and $_.Connected })
if (-not $back) { Fail 'criterion 6: never reconnected after the relay returned' }
else {
    $first = $back | Select-Object -First 1
    $last = $back | Select-Object -Last 1
    $before = ($connectedEarly | Select-Object -Last 1).Generation
    Write-Host "criterion 6: reconnected by t=$([int]$first.Time)s, generation $before then $($first.Generation), reconnects=$($first.Reconnects)"
    if ($first.Generation -le $before) { Fail 'criterion 6: the connection generation did not advance on reconnect' }
    if ($first.Reconnects -lt 1) { Fail 'criterion 6: the reconnect was not counted' }
    if ($last.Bytes -le ($connectedEarly | Select-Object -Last 1).Bytes) {
        Fail 'criterion 6: reconnected but no new bytes arrived'
    } else {
        Write-Host "  and bytes resumed, $(($connectedEarly | Select-Object -Last 1).Bytes) before the kill to $($last.Bytes) after"
    }
    # The backoff must go back to its first interval, or a link that drops once a minute creeps up
    # to the ceiling and stays there.
    if ($first.Backoff -ne 500) { Fail "criterion 6: the backoff did not reset, it is $($first.Backoff)ms" }
}

Write-Host ''
Write-Host "Connector gate: $failures failures"
if ($failures) { exit 1 }
exit 0
