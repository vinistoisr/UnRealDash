# PLAN 4.7's command-line surface gate, invoked by capture-metrics.ps1 -Smoke.
#
# Every documented flag is exercised once and its resolved value and source are read back out of
# the player's own log. That per-field source line is the whole point: a flag that parsed but did
# not take effect reads as source=default, which is the only way a typo in the flat namespace is
# caught at all. See chunk-16-command-line.md revision 2 C1 for why the command-line unknown-flag
# check cannot cover that namespace.
[CmdletBinding()]
param(
    [string]$Player = (Join-Path $PSScriptRoot '../runtime/UnRealDash/Saved/StagedBuilds/Windows/UnRealDash.exe'),
    [string]$Package = (Join-Path $PSScriptRoot '../tests/fixtures/packages/dials-stage0.udash'),
    [string]$WorkDirectory = (Join-Path $PSScriptRoot '../.tmp/flag-gate')
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
$log = Join-Path $saved 'Logs/UnRealDash.log'
$configPath = Join-Path $saved 'player.json'
New-Item -ItemType Directory -Force -Path $saved | Out-Null

# The player reads player.json from its own Saved directory, so a run that is meant to take
# everything from the command line has to start from a clean slate or the file would leak into it.
function Clear-Config { if (Test-Path $configPath) { Remove-Item $configPath -Force } }

# Bounded, not -Wait. A flag the engine also owns can stop the player reaching BeginPlay at all,
# which is not a theory: -replay= put it inside UGameInstance's replay system and the first run of
# this gate hung for ten minutes. A hang is a failure and has to be reported as one.
function Invoke-Player([string[]]$Extra, [int]$TimeoutSeconds = 90) {
    # A player left running by an earlier hang holds the log open, the delete fails, and every
    # later run then reads the hung run's output instead of its own. That happened, and it turned
    # one real failure into twenty-five imaginary ones. So the delete is checked, not attempted.
    if (Test-Path $log) {
        Remove-Item $log -Force -ErrorAction SilentlyContinue
        if (Test-Path $log) {
            Fail 'the previous log could not be deleted, so a process is still holding it'
            Get-Process UnRealDash -ErrorAction SilentlyContinue | Stop-Process -Force
            Start-Sleep -Seconds 2
            Remove-Item $log -Force -ErrorAction SilentlyContinue
        }
    }
    $arguments = @('-windowed', '-ResX=640', '-ResY=360', '-nosplash', '-unattended') + $Extra
    $process = Start-Process -FilePath $Player -ArgumentList $arguments -PassThru
    $code = -1
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        Fail "the player did not exit within $TimeoutSeconds s of: $($Extra -join ' ')"
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        Start-Sleep -Milliseconds 500
    } else { $code = $process.ExitCode }
    $lines = if (Test-Path $log) { Get-Content $log } else { @() }
    return [pscustomobject]@{ ExitCode = $code; Lines = $lines }
}

# The resolver logs one line per field: config <name>=<value> source=<where>.
function Get-Field([string[]]$Lines, [string]$Name) {
    foreach ($line in $Lines) {
        # The source is matched greedily to the end of the line, not as a word: "player.json" has
        # a dot in it, and a \w class silently matched only "command line" and nothing else.
        if ($line -match "config $([regex]::Escape($Name))=(.*) source=(.+)$") {
            return [pscustomobject]@{ Value = $Matches[1]; Source = $Matches[2].Trim() }
        }
    }
    return $null
}

Write-Host 'every documented flag, set from the command line:'
Clear-Config
$matrix = @(
    @{ Flag = 'udash';                Value = $Package;         Expect = $Package }
    @{ Flag = 'scenario';             Value = 'acceleration';   Expect = 'acceleration' }
    @{ Flag = 'replay-file';          Value = 'C:/none.bin';    Expect = 'C:/none.bin' }
    @{ Flag = 'connector';            Value = 'sim';            Expect = 'sim' }
    @{ Flag = 'connector-host';       Value = '127.0.0.2';      Expect = '127.0.0.2' }
    @{ Flag = 'connector-port';       Value = '35001';          Expect = '35001' }
    @{ Flag = 'screenshot';           Value = (Join-Path $WorkDirectory 'flags.png'); Expect = (Join-Path $WorkDirectory 'flags.png') }
    @{ Flag = 'quit-after';           Value = '3';              Expect = '3' }
    @{ Flag = 'profile';              Value = 'desktop';        Expect = 'desktop' }
    @{ Flag = 'ack-warnings-at';      Value = '1.5';            Expect = '1.5' }
    @{ Flag = 'reveal-delay-ms';      Value = '250';            Expect = '250' }
    @{ Flag = 'rhi';                  Value = 'vulkan';         Expect = 'vulkan' }
)
$valued = @($matrix | ForEach-Object { "-$($_.Flag)=$($_.Value)" })
# The two switches carry no value.
$run = Invoke-Player ($valued + @('-replay-loop', '-freeze-scenario-time'))
foreach ($entry in $matrix) {
    $field = Get-Field $run.Lines $entry.Flag
    if (-not $field) { Fail "$($entry.Flag): the player logged no value for it" ; continue }
    if ($field.Value -ne $entry.Expect) { Fail "$($entry.Flag)=$($field.Value), expected $($entry.Expect)"; continue }
    if ($field.Source -ne 'command line') { Fail "$($entry.Flag) came from $($field.Source), not the command line"; continue }
    Write-Host "  -$($entry.Flag)=$($field.Value) source=$($field.Source)"
}
foreach ($switch in @('replay-loop', 'freeze-scenario-time')) {
    $field = Get-Field $run.Lines $switch
    if (-not $field) { Fail "${switch}: the player logged no value for it" }
    elseif ($field.Value -ne 'true') { Fail "${switch} resolved $($field.Value), expected true" }
    else { Write-Host "  -$switch=$($field.Value) source=$($field.Source)" }
}
# And the accepted-but-unconsumed flags say so rather than looking like they did something.
foreach ($name in @('ack-warnings-at', 'reveal-delay-ms', 'rhi')) {
    if (-not ($run.Lines | Select-String "config $name is accepted but not consumed yet")) {
        Fail "$name did not say it is accepted but not consumed"
    }
}

Write-Host ''
Write-Host 'an unknown flag in this project namespace:'
Clear-Config
$run = Invoke-Player @("-udash=$Package", '-udash-typo=1', '-quit-after=2')
if ($run.ExitCode -eq 0) { Fail 'an unknown -udash flag exited 0' }
elseif (-not ($run.Lines | Select-String 'No such flag: -udash-typo')) { Fail 'the unknown flag was not named' }
elseif (-not ($run.Lines | Select-String 'Supported flags')) { Fail 'the supported list was not printed' }
else { Write-Host "  rejected, exit $($run.ExitCode), and the supported list was printed" }

Write-Host ''
Write-Host 'an unknown player.json key:'
'{ "udash": "x", "scenarioo": "acceleration" }' | Set-Content $configPath -Encoding utf8
$run = Invoke-Player @('-quit-after=2')
if ($run.ExitCode -eq 0) { Fail 'an unknown player.json key exited 0' }
elseif (-not ($run.Lines | Select-String 'player.json has no such key: scenarioo')) { Fail 'the unknown key was not named' }
else { Write-Host "  rejected, exit $($run.ExitCode)" }

Write-Host ''
Write-Host 'the command line wins field by field:'
@{ udash = 'from-file'; scenario = 'idle'; 'connector-host' = '10.0.0.1'; 'connector-port' = 4242 } |
    ConvertTo-Json | Set-Content $configPath -Encoding utf8
$run = Invoke-Player @("-udash=$Package", '-quit-after=2')
$overridden = Get-Field $run.Lines 'udash'
$kept = @('scenario', 'connector-host', 'connector-port') | ForEach-Object { Get-Field $run.Lines $_ }
if (-not $overridden -or $overridden.Source -ne 'command line') { Fail 'the overridden field did not come from the command line' }
elseif ($overridden.Value -ne $Package) { Fail "udash resolved $($overridden.Value)" }
else { Write-Host "  udash=$($overridden.Value) source=command line" }
foreach ($field in $kept) {
    if (-not $field -or $field.Source -ne 'player.json') { Fail 'a field the command line did not set was lost' }
}
if ($kept.Count -eq 3 -and -not ($kept | Where-Object { $_.Source -ne 'player.json' })) {
    Write-Host "  and the other three still came from player.json: $(($kept | ForEach-Object { $_.Value }) -join ', ')"
}

Write-Host ''
Write-Host 'a missing player.json still runs:'
Clear-Config
$run = Invoke-Player @("-udash=$Package", '-quit-after=2')
if ($run.ExitCode -ne 0) { Fail "a missing player.json exited $($run.ExitCode)" }
else { Write-Host '  ran with the command line and the defaults' }

Write-Host ''
Write-Host 'an invalid field does not:'
Clear-Config
$run = Invoke-Player @("-udash=$Package", '-connector=nonsense', '-quit-after=2')
if ($run.ExitCode -eq 0) { Fail 'an invalid connector exited 0' }
else { Write-Host "  rejected, exit $($run.ExitCode)" }

# The reproduction check runs a deterministic configuration with no connector, where two captures
# of one document are bit identical. With a connector it would measure the connector's timing
# rather than whether the two configuration paths agree. See chunk-16 revision 2 C2.
Write-Host ''
Write-Host 'the same run reproduces from the command line and from player.json:'
$fromCli = Join-Path $WorkDirectory 'cli.png'
$fromFile = Join-Path $WorkDirectory 'file.png'
Clear-Config
Invoke-Player @("-udash=$Package", "-screenshot=$fromCli", '-quit-after=6') | Out-Null
@{ udash = $Package; screenshot = $fromFile; 'quit-after' = 6 } | ConvertTo-Json | Set-Content $configPath -Encoding utf8
Invoke-Player @() | Out-Null
Clear-Config
if (-not (Test-Path $fromCli) -or -not (Test-Path $fromFile)) { Fail 'one of the two runs produced no capture' }
else {
    $output = & python (Join-Path $PSScriptRoot 'compare-capture.py') $fromCli $fromFile 2>&1
    Write-Host "  $($output -join "`n")"
    if ($LASTEXITCODE -ne 0) { Fail 'the two configuration paths produced different frames' }
}

Write-Host ''
Write-Host "Flag gate: $failures failures"
if ($failures) { exit 1 }
exit 0
