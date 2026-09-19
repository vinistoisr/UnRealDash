# PLAN 4.10: census and validation name the same document the player is measured against.
[CmdletBinding()]
param(
    [string]$Player = (Join-Path $PSScriptRoot '../runtime/UnRealDash/Saved/StagedBuilds/Windows/UnRealDash.exe'),
    [string]$Package = (Join-Path $PSScriptRoot '../tests/fixtures/packages/control-dashboard.udash'),
    [string]$SourceDirectory = (Join-Path $PSScriptRoot '../examples/control-dashboard'),
    [string]$WorkDirectory = (Join-Path $PSScriptRoot '../.tmp/example-gate'),
    [string]$PythonExe = 'python'
)
$ErrorActionPreference = 'Stop'
$failures = 0
function Fail([string]$Message) { $script:failures++; Write-Host "FAIL $Message" }
$corpus = Join-Path $PSScriptRoot '../tests/fixtures/documents'
$bundle = Join-Path $corpus 'valid/control-dashboard.json'
$helper = Join-Path $PSScriptRoot 'check-example.py'
$exec = 'r.PostProcessAAQuality 0, r.DefaultFeature.AntiAliasing 0, r.DefaultFeature.Bloom 0, ' +
        'r.DefaultFeature.AutoExposure 0, r.DefaultFeature.MotionBlur 0, r.ScreenPercentage 100, ' +
        'r.Tonemapper.Sharpen 0'

try {
    New-Item -ItemType Directory -Force -Path $WorkDirectory | Out-Null
    $WorkDirectory = (Resolve-Path -LiteralPath $WorkDirectory).Path
    foreach ($path in @($Package, $SourceDirectory, $bundle)) {
        if (-not (Test-Path -LiteralPath $path)) { Write-Host "Missing: $path. Run tools/gen-control-dashboard.py"; exit 2 }
    }
    $Package = (Resolve-Path -LiteralPath $Package).Path
    Write-Host 'criteria 1, 2, 5: bound census, declarations, ranges and source/package/corpus agreement'
    & $PythonExe $helper document --source $SourceDirectory --corpus $bundle --package $Package
    if ($LASTEXITCODE -ne 0) { Fail 'document checks'; exit 1 }

    Write-Host 'criterion 3: existing validator corpus path'
    $report = Join-Path $WorkDirectory 'corpus.tsv'
    if (Test-Path -LiteralPath $report) { Remove-Item -LiteralPath $report -Force }
    & $PythonExe (Join-Path $PSScriptRoot '../tools/validate-dashboard.py') $corpus --report $report
    if ($LASTEXITCODE -ne 0) { Fail 'validator corpus' }
    if (-not (Test-Path -LiteralPath $report)) { Fail 'missing corpus report' }
    elseif (-not (Select-String -LiteralPath $report -Pattern '^valid/control-dashboard\.json\tPASS\t\t$')) {
        Fail 'the corpus did not report this example as PASS'
    }
    if (-not (Test-Path -LiteralPath $Player)) { Write-Host "No packaged player at $Player"; exit 2 }
    $Player = (Resolve-Path -LiteralPath $Player).Path

    function Invoke-Capture([string]$Name, [string]$Profile, [string]$State, [string]$At, [bool]$Sweep) {
        $output = Join-Path $WorkDirectory ($Name + '.png')
        $log = Join-Path $WorkDirectory ($Name + '.log')
        foreach ($path in @($output, $log)) {
            if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Force }
        }
        # Quoted values survive Start-Process joining ArgumentList when the workspace has spaces.
        $arguments = @('-windowed', '-ResX=1280', '-ResY=720', '-nosplash', '-unattended',
            ('-udash="{0}"' -f $Package), ('-screenshot="{0}"' -f $output),
            ('-abslog="{0}"' -f $log), "-profile=$Profile", "-udash-state=$State",
            "-udash-shot-at=$At", '-quit-after=12', ('-ExecCmds="{0}"' -f $exec))
        if ($Sweep) { $arguments += '-udash-sweep=8' }
        else { $arguments += '-udash-fraction=0.6' }
        $process = Start-Process -FilePath $Player -ArgumentList $arguments -WindowStyle Hidden -PassThru
        if (-not $process.WaitForExit(90000)) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
            Fail "$Name timed out after 90 seconds"
            return $null
        }
        if ($process.ExitCode -ne 0) { Fail "$Name exited $($process.ExitCode)"; return $null }
        if (-not (Test-Path -LiteralPath $log)) { Fail "$Name produced no log"; return $null }
        $verdicts = @(Select-String -LiteralPath $log -Pattern 'DashVerdict accepted=')
        if ($verdicts.Count -ne 1 -or $verdicts[0].Line -notmatch 'DashVerdict accepted=true\s') {
            Fail "$Name did not report exactly one accepted=true verdict"
            return $null
        }
        Write-Host "$Name ($Profile): $($verdicts[0].Line)"
        $connectorPattern = 'DashConnector kind=none\s*$'
        if ($Sweep) { $connectorPattern = 'DashConnector kind=sweep\s' }
        if (-not (Select-String -LiteralPath $log -Pattern $connectorPattern)) {
            Fail "$Name did not use the intended source; check the player's Saved/player.json"
            return $null
        }
        if (-not (Test-Path -LiteralPath $output)) { Fail "$Name produced no capture"; return $null }
        & $PythonExe $helper capture --source $SourceDirectory --first $output | Out-Host
        if ($LASTEXITCODE -ne 0) { Fail "$Name criterion 4"; return $null }
        return $output
    }

    # Opposite slopes avoid an auto-scaled graph rendering two identical rising ramps. These
    # points also differ at every quarter-cycle phase used by the player's per-signal sweep.
    $early = Invoke-Capture 'desktop-early' 'desktop' 'valid' '0.5' $true
    $late = Invoke-Capture 'desktop-late' 'desktop' 'valid' '5.0' $true
    $mobile = Invoke-Capture 'mobile' 'mobile' 'valid' '0.5' $true
    if ($early -and $late) {
        Write-Host 'criterion 6: every value instrument changes inside its own rect'
        & $PythonExe $helper instruments --source $SourceDirectory --first $early --second $late
        if ($LASTEXITCODE -ne 0) { Fail 'instrument motion' }
    }

    # A live source would overwrite the forced quality, so the indicator pair has no sweep.
    $valid = Invoke-Capture 'indicators-valid' 'desktop' 'valid' '1.0' $false
    $unavailable = Invoke-Capture 'indicators-unavailable' 'desktop' 'unavailable' '1.0' $false
    if ($valid -and $unavailable) {
        Write-Host 'criterion 6b: every indicator changes with unavailable quality'
        & $PythonExe $helper indicators --source $SourceDirectory --first $valid --second $unavailable
        if ($LASTEXITCODE -ne 0) { Fail 'indicator quality' }
    }
} catch {
    Fail $_.Exception.Message
}
Write-Host "Example gate: $failures failures"
if ($failures) { exit 1 }
exit 0
