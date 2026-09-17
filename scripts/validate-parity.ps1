param(
    [string]$PythonExe = 'python',
    [string]$ValidatorExe = "$PSScriptRoot/../packages/dashboard-spec/build/default/dashboard-spec-validate.exe",
    [string]$CorpusPath = "$PSScriptRoot/../tests/fixtures/documents"
)
$ErrorActionPreference = 'Stop'
$pythonReport = [IO.Path]::GetTempFileName()
$cppReport = [IO.Path]::GetTempFileName()
try {
    & $PythonExe "$PSScriptRoot/../tools/validate-dashboard.py" $CorpusPath --report $pythonReport
    $pythonExit = $LASTEXITCODE
    & $ValidatorExe $CorpusPath --report $cppReport
    $cppExit = $LASTEXITCODE
    $left = [IO.File]::ReadAllLines($pythonReport)
    $right = [IO.File]::ReadAllLines($cppReport)
    $differences = 0
    for ($i = 0; $i -lt [Math]::Max($left.Length, $right.Length); $i++) {
        $a = if ($i -lt $left.Length) { $left[$i] } else { '<missing>' }
        $b = if ($i -lt $right.Length) { $right[$i] } else { '<missing>' }
        if ($a -cne $b) {
            $differences++
            Write-Output "Python: $a"
            Write-Output "C++:    $b"
        }
    }
    Write-Output "Parity diff: $differences differing lines"
    if ($differences -or $pythonExit -or $cppExit) { exit 1 }
    exit 0
} finally {
    Remove-Item -LiteralPath $pythonReport, $cppReport -Force
}
