# The PLAN 4.6 gate: rendering the same document at 1280x720 and at 1280x660 produces a dial whose
# measured width to height ratio stays within 1 percent of 1.0 in both captures.
#
# This is a measurement, not a comparison. A dial scaled non-uniformly still matches its own
# reference perfectly, so scripts/compare-capture.py cannot see the defect this gate exists to
# catch. scripts/measure-extent.py measures instead, finding each component by a colour the gate
# fixture gives to nothing else.
#
# Like the chunk 11 capture gate, this does not run in CI and must not be made to.
[CmdletBinding()]
param(
    [string]$Player = (Join-Path $PSScriptRoot '../runtime/UnRealDash/Saved/StagedBuilds/Windows/UnRealDash.exe'),
    [string]$PackageDirectory = (Join-Path $PSScriptRoot '../tests/fixtures/packages'),
    [string]$WorkDirectory = (Join-Path $PSScriptRoot '../.tmp/dial-gate')
)
$ErrorActionPreference = 'Stop'

$exec = 'r.PostProcessAAQuality 0, r.DefaultFeature.AntiAliasing 0, r.DefaultFeature.Bloom 0, ' +
        'r.DefaultFeature.AutoExposure 0, r.DefaultFeature.MotionBlur 0, r.ScreenPercentage 100, ' +
        'r.Tonemapper.Sharpen 0'

# Colours, and the bands they are measured against. Both bands come from the arithmetic in
# tools/gen-chunk12-fixture.py, which is the only place the fixture geometry is pinned.
#
#   face   600 units -> 550 px at 660. A 1 percent band is 5.50 px against at most 2 px of
#          bounding-box quantisation, which is 0.73 percent. PLAN 4.6 asks for 1 percent and the
#          fixture is sized so 1 percent is defensible.
#   needle 191 units -> 175 px at 660. A 1 percent band would be 1.75 px against the same 2 px,
#          which is 2.31 percent, so a 1 percent band there could fail a correct render. The needle
#          carries 3 percent, and a stretch still shows 9.09 percent, a margin of three times.
$faceColour = '#2244aa'
$needleColour = '#ff2200'
$faceBand = 0.01
$needleBand = 0.03
$traceColours = @{ arc = '#00cc66'; bar = '#ffcc00'; trace = '#cc00ff' }

$failures = 0
function Fail([string]$message) { $script:failures++; Write-Host "FAIL $message" }

New-Item -ItemType Directory -Force -Path $WorkDirectory | Out-Null
if (-not (Test-Path $Player)) { Write-Host "No packaged player at $Player"; exit 2 }

function Invoke-Capture([string]$Package, [int]$Height, [string]$Fraction, [string]$Output) {
    if (Test-Path $Output) { Remove-Item $Output -Force }
    $arguments = @('-windowed', '-ResX=1280', "-ResY=$Height", '-nosplash', '-unattended',
        "-udash=$Package", "-udash-fraction=$Fraction", "-screenshot=$Output", "-ExecCmds=$exec")
    $process = Start-Process -FilePath $Player -ArgumentList $arguments -PassThru -Wait
    if ($process.ExitCode -ne 0) { Fail "capture at ${Height}p exited $($process.ExitCode)"; return $false }
    if (-not (Test-Path $Output)) { Fail "capture at ${Height}p produced no file"; return $false }
    return $true
}

# Returns the measured ratio, or NaN when nothing was measured. A measurement of nothing is never
# silently treated as a ratio of 1.0.
function Measure-Ratio([string]$Capture, [string]$Colour, [int]$Floor) {
    $output = & python (Join-Path $PSScriptRoot 'measure-extent.py') $Capture --colour $Colour `
        --minimum-pixels $Floor 2>&1
    $text = ($output -join "`n")
    Write-Host "  $text"
    if ($LASTEXITCODE -ne 0) { return [pscustomobject]@{ Ratio = [double]::NaN; Text = $text } }
    if ($text -match 'ratio=([0-9.]+)') { return [pscustomobject]@{ Ratio = [double]$Matches[1]; Text = $text } }
    return [pscustomobject]@{ Ratio = [double]::NaN; Text = $text }
}

$package = Join-Path $PackageDirectory 'dials-stage0.udash'
if (-not (Test-Path $package)) { Write-Host "No fixture at $package. Run tools/gen-chunk12-fixture.py"; exit 2 }

# Criteria 2 and 3: the dial keeps its shape at both viewport heights. Fraction 0 puts the needle at
# 225 degrees, which is a diagonal, where the bounding box of a rotated rectangle is square.
foreach ($height in @(720, 660)) {
    $capture = Join-Path $WorkDirectory "dials-$height.png"
    if (-not (Invoke-Capture $package $height '0.0' $capture)) { continue }
    Write-Host "at ${height}p:"

    $face = Measure-Ratio $capture $faceColour 10000
    if ([double]::IsNaN($face.Ratio)) { Fail "criterion 3: the face was not measurable at ${height}p" }
    elseif ([Math]::Abs($face.Ratio - 1.0) -gt $faceBand) {
        Fail ("criterion 3: face ratio {0:F6} at ${height}p is outside 1 percent of 1.0" -f $face.Ratio)
    } else {
        Write-Host ("  criterion 3 face at ${height}p: ratio {0:F6}, {1:F2} percent off 1.0, band 1 percent" -f `
            $face.Ratio, ([Math]::Abs($face.Ratio - 1.0) * 100))
    }

    $needle = Measure-Ratio $capture $needleColour 1000
    if ([double]::IsNaN($needle.Ratio)) { Fail "criterion 2: the needle was not measurable at ${height}p" }
    elseif ([Math]::Abs($needle.Ratio - 1.0) -gt $needleBand) {
        Fail ("criterion 2: needle ratio {0:F6} at ${height}p is outside 3 percent of 1.0" -f $needle.Ratio)
    } else {
        Write-Host ("  criterion 2 needle at ${height}p: ratio {0:F6}, {1:F2} percent off 1.0, band 3 percent" -f `
            $needle.Ratio, ([Math]::Abs($needle.Ratio - 1.0) * 100))
    }
}

# Criterion 4: the control, in two halves that differ by exactly one field. The same document with
# the face in a 600 by 400 rect must FAIL the band under aspect_policy stretch and PASS it under
# preserve. A gate nobody has watched fail is not a gate, and this project has already shipped two
# assertions that could not fail.
#
# Note what this control is NOT. An earlier version set the ROOT component's scaling to stretch,
# expecting EStretch::Fill to scale the document non-uniformly, which is how the engine documents
# it. Measured, it did not: the document still scaled uniformly. That is recorded as an open finding
# in the chunk 12 report and is why the control tests aspect_policy, which was measured to work,
# rather than scaling, which was measured not to.
$stretched = Join-Path $PackageDirectory 'dials-stage0-stretched.udash'
$preserved = Join-Path $PackageDirectory 'dials-stage0-preserved.udash'
foreach ($path in @($stretched, $preserved)) {
    if (-not (Test-Path $path)) { Write-Host "No control fixture at $path. Run tools/gen-chunk12-fixture.py"; exit 2 }
}

$control = Join-Path $WorkDirectory 'control-stretched.png'
if (Invoke-Capture $stretched 720 '0.0' $control) {
    Write-Host 'criterion 4a, face aspect_policy stretch, which must FAIL the band:'
    $face = Measure-Ratio $control $faceColour 10000
    if ([double]::IsNaN($face.Ratio)) {
        Fail 'criterion 4a: the control face was not measurable, so the control proves nothing'
    } elseif ([Math]::Abs($face.Ratio - 1.0) -le $faceBand) {
        Fail ("criterion 4a: the stretched control PASSED at ratio {0:F6}, so criteria 2 and 3 prove nothing" -f $face.Ratio)
    } else {
        Write-Host ("  criterion 4a: ratio {0:F6}, {1:F2} percent off 1.0, correctly outside the band" -f `
            $face.Ratio, ([Math]::Abs($face.Ratio - 1.0) * 100))
    }
}

$control = Join-Path $WorkDirectory 'control-preserved.png'
if (Invoke-Capture $preserved 720 '0.0' $control) {
    Write-Host 'criterion 4b, the same rect under preserve, which must PASS it:'
    $face = Measure-Ratio $control $faceColour 10000
    if ([double]::IsNaN($face.Ratio)) { Fail 'criterion 4b: the control face was not measurable' }
    elseif ([Math]::Abs($face.Ratio - 1.0) -gt $faceBand) {
        Fail ("criterion 4b: preserve in a non-square rect gave ratio {0:F6}, which is not square" -f $face.Ratio)
    } else {
        Write-Host ("  criterion 4b: ratio {0:F6}, square inside a 600 by 400 rect, so aspect_policy is what decides" -f $face.Ratio)
    }
}

# Criterion 5: the circular bar gauge and the linear bar gauge each draw something, at a deflection
# where each has a visible extent. The floors are recorded here.
#
# The history graph is NOT measured here any more, and that is a deliberate reduction in what this
# gate covers. Chunk 12 filled its buffer at build time from -udash-fraction, so a static capture
# could measure a trace. Chunk 14 removed that: a history graph has no samples until a signal
# arrives, and a synthetic ramp standing in for a series was inventing data. With no live source in
# this gate the trace is correctly empty, so measuring it here could only be satisfied by putting
# the invented data back. Its coverage moves to the chunk 14 live gate, where a real series drives
# it, and this comment is here so the gap is visible rather than quietly dropped.
$deflected = Join-Path $WorkDirectory 'dials-720-deflected.png'
if (Invoke-Capture $package 720 '0.6' $deflected) {
    Write-Host 'criterion 5, every remaining primitive draws:'
    # Floors chosen well under the measured counts but far above zero, so a primitive that stopped
    # drawing fails rather than measuring a stray pixel and passing.
    foreach ($entry in @(@('arc', 3000), @('bar', 10000))) {
        $result = Measure-Ratio $deflected $traceColours[$entry[0]] $entry[1]
        if ([double]::IsNaN($result.Ratio)) { Fail "criterion 5: $($entry[0]) drew fewer than $($entry[1]) pixels" }
    }
}

Write-Host ''
Write-Host "Dial gate: $failures failures"
if ($failures) { exit 1 }
exit 0
