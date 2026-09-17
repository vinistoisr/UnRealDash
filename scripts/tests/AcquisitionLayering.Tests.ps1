BeforeAll {
    . (Join-Path $PSScriptRoot '../SignalCoreLayering.ps1')
}
Describe 'PLAN 4.3 SignalCore layering' {
    It 'names file and line for each forbidden token outside the allowed modules' {
        $source = Join-Path $TestDrive 'Source'
        foreach ($module in @('SignalCore', 'UnRealDashCore', 'Game')) {
            New-Item -ItemType Directory -Path (Join-Path $source $module) -Force | Out-Null
        }
        $text = "#include `"SignalCore/Sample.h`"`nsignal_core::Sample Sample;`nusing namespace signal_core;"
        foreach ($module in @('SignalCore', 'UnRealDashCore', 'Game')) {
            Set-Content -LiteralPath (Join-Path $source "$module/test.cpp") -Value $text
        }
        $found = @(Get-SignalCoreLayeringViolation $source)
        $found.Count | Should -Be 3
        $found[0] | Should -Match 'Game[\\/]test.cpp:1:'
        $found[1] | Should -Match ':2:'
        $found[2] | Should -Match ':3:'
    }
    It 'accepts the engine sample seam and the actual source tree' {
        $root = Join-Path $PSScriptRoot '../../runtime/UnRealDash/Source'
        @(Get-SignalCoreLayeringViolation $root).Count | Should -Be 0
        $headers = Get-ChildItem (Join-Path $root 'UnRealDashCore/Public') -Recurse -Filter '*.h'
        @($headers | Select-String -Pattern 'signal_core::|#\s*include\s*"SignalCore/').Count | Should -Be 0
    }
    It 'keeps the layering gate wired into doctor' {
        Get-Content (Join-Path $PSScriptRoot '../doctor.ps1') -Raw | Should -Match 'Get-SignalCoreLayeringViolation'
    }
}
