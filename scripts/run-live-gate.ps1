# The chunk 14 live-binding gate: PLAN 4.9's "the player shows live values", and the part of it
# that chunk 13 could not reach.
#
# It captures one running scenario at two points and measures what moved. The chunk 11 comparator
# cannot answer this: two frames of a moving dashboard differ everywhere, and "differs" is not the
# question. The question is whether the BOUND components moved and the unbound one did not.
[CmdletBinding()]
param(
    [string]$Player = (Join-Path $PSScriptRoot '../runtime/UnRealDash/Saved/StagedBuilds/Windows/UnRealDash.exe'),
    [string]$Package = (Join-Path $PSScriptRoot '../tests/fixtures/packages/live-stage0.udash'),
    [string]$WorkDirectory = (Join-Path $PSScriptRoot '../.tmp/live-gate')
)
$ErrorActionPreference = 'Stop'

$exec = 'r.PostProcessAAQuality 0, r.DefaultFeature.AntiAliasing 0, r.DefaultFeature.Bloom 0, ' +
        'r.DefaultFeature.AutoExposure 0, r.DefaultFeature.MotionBlur 0, r.ScreenPercentage 100, ' +
        'r.Tonemapper.Sharpen 0'

# Shared with the dial fixture, so a measurement here means what it means there.
$needle = '#ff2200'
$face = '#2244aa'
$arc = '#00cc66'
$stale = '#c8501e'

# The scenario sweeps a triangle over its whole duration, so these two points are far apart in it
# and the needle is nowhere near the same place at both.
$scenario = 8
$early = 0.5
$late = 3.5

$failures = 0
function Fail([string]$message) { $script:failures++; Write-Host "FAIL $message" }

New-Item -ItemType Directory -Force -Path $WorkDirectory | Out-Null
if (-not (Test-Path $Player)) { Write-Host "No packaged player at $Player"; exit 2 }
if (-not (Test-Path $Package)) { Write-Host "No fixture at $Package. Run tools/gen-chunk14-fixture.py"; exit 2 }

function Invoke-Capture([string]$Output, [string]$At, [string]$Seconds) {
    if (Test-Path $Output) { Remove-Item $Output -Force }
    $arguments = @('-windowed', '-ResX=1280', '-ResY=720', '-nosplash', '-unattended',
        "-udash=$Package", "-udash-scenario=$Seconds", "-udash-shot=$Output", "-udash-shot-at=$At",
        "-ExecCmds=$exec")
    $process = Start-Process -FilePath $Player -ArgumentList $arguments -PassThru -Wait
    if ($process.ExitCode -ne 0) { Fail "capture at $At s exited $($process.ExitCode)"; return $false }
    if (-not (Test-Path $Output)) { Fail "capture at $At s produced no file"; return $false }
    return $true
}

# Returns the bounding box as a string and the pixel count, or nulls when nothing was measured. A
# measurement of nothing is never silently treated as agreement.
function Measure-Box([string]$Capture, [string]$Colour, [int]$Floor) {
    $output = & python (Join-Path $PSScriptRoot 'measure-extent.py') $Capture --colour $Colour `
        --minimum-pixels $Floor 2>&1
    $text = ($output -join "`n")
    Write-Host "  $text"
    if ($LASTEXITCODE -ne 0) { return $null }
    if ($text -match 'box=\((\d+), (\d+), (\d+), (\d+)\).*pixels=(\d+)') {
        return [pscustomobject]@{
            Box = "$($Matches[1]),$($Matches[2]),$($Matches[3]),$($Matches[4])"
            Left = [int]$Matches[1]; Top = [int]$Matches[2]
            Pixels = [int]$Matches[5]
        }
    }
    return $null
}

$earlyShot = Join-Path $WorkDirectory 'early.png'
$lateShot = Join-Path $WorkDirectory 'late.png'
Write-Host "capturing the same scenario at $early s and $late s"
$haveEarly = Invoke-Capture $earlyShot $early $scenario
$haveLate = Invoke-Capture $lateShot $late $scenario

if ($haveEarly -and $haveLate) {
    # Criterion 2: a bound gauge moves. Measured as the needle's bounding box landing somewhere
    # else, not as the frame differing, because a frame differs for any reason at all.
    Write-Host 'criterion 2, the bound needle moves:'
    $a = Measure-Box $earlyShot $needle 500
    $b = Measure-Box $lateShot $needle 500
    if (-not $a -or -not $b) { Fail 'criterion 2: the needle was not measurable in both captures' }
    else {
        # Ten pixels on both axes, which is far more than the couple of pixels a bounding box can
        # move from quantisation and far less than the hundreds a real sweep produces.
        $moved = ([Math]::Abs($a.Left - $b.Left) -gt 10) -or ([Math]::Abs($a.Top - $b.Top) -gt 10)
        Write-Host "  early box $($a.Box), late box $($b.Box)"
        if (-not $moved) { Fail "criterion 2: the needle did not move between $early s and $late s" }
        else { Write-Host '  the needle is in a different place, so the binding drives it' }
    }

    # Criterion 3: the control. An unbound component must NOT move, or the gate would pass on a
    # tree that rebuilt itself rather than on a binding that worked.
    Write-Host 'criterion 3, the unbound face does not move:'
    $a = Measure-Box $earlyShot $face 10000
    $b = Measure-Box $lateShot $face 10000
    if (-not $a -or -not $b) { Fail 'criterion 3: the face was not measurable in both captures' }
    elseif ($a.Box -ne $b.Box) { Fail "criterion 3: the unbound face moved from $($a.Box) to $($b.Box)" }
    else { Write-Host "  identical box $($a.Box) in both" }

    # And the circular gauge, which is driven by the same signal through a different primitive.
    Write-Host 'criterion 2b, the circular gauge sweeps:'
    $a = Measure-Box $earlyShot $arc 500
    $b = Measure-Box $lateShot $arc 500
    if (-not $a -or -not $b) { Fail 'criterion 2b: the arc was not measurable in both captures' }
    elseif ($a.Pixels -eq $b.Pixels) { Fail 'criterion 2b: the arc drew the same number of pixels at both points' }
    else { Write-Host "  $($a.Pixels) pixels early, $($b.Pixels) late" }
}

# Criterion 4: a stopped source goes stale. The scenario is short and the capture is taken well
# after it ends, so every signal has passed its 500 ms deadline in the real registry. The dial's
# stale presentation is dash, which hides the needle and shows the band, so the measurement is the
# band appearing in the stale token colour and the needle being gone.
$staleShot = Join-Path $WorkDirectory 'stale.png'
Write-Host 'criterion 4, the source stops and the signal goes stale:'
if (Invoke-Capture $staleShot 4 1) {
    $band = Measure-Box $staleShot $stale 5000
    if (-not $band) { Fail 'criterion 4: no stale band appeared after the scenario ended' }
    else { Write-Host "  the stale band is present, $($band.Pixels) pixels" }

    $gone = & python (Join-Path $PSScriptRoot 'measure-extent.py') $staleShot --colour $needle --minimum-pixels 1 2>&1
    if ($LASTEXITCODE -eq 0) { Fail 'criterion 4: the needle is still drawn for a stale signal' }
    else { Write-Host '  and the needle is gone, which is what the dash presentation means' }
}

Write-Host ''
Write-Host "Live gate: $failures failures"
if ($failures) { exit 1 }
exit 0
