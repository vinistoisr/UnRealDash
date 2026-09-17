BeforeAll {
    $root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
    $pass = Join-Path $PSScriptRoot 'fakes/doctor-pass.ps1'
    $fail = Join-Path $PSScriptRoot 'fakes/doctor-fail.ps1'
    $generated = Join-Path $root 'runtime/UnRealDash/Config/GeneratedEngine.ini'
    . (Join-Path $root 'scripts/lib/package.ps1')
    function Invoke-PackagePreview($Script, $Arguments) {
        $lines = & (Resolve-PwshPath) -NoProfile -File (Join-Path $root "scripts/$Script.ps1") @Arguments 2>&1
        @{ Code = $LASTEXITCODE; Text = $lines -join "`n" }
    }
}
Describe 'Smoke packaging' {
    It 'previews complete <Platform> <Configuration> packaging' -ForEach @(
        @{ Script='package-windows'; Platform='Win64'; Configuration='Development'; Rhi='' }
        @{ Script='package-windows'; Platform='Win64'; Configuration='Shipping'; Rhi='' }
        @{ Script='package-android'; Platform='Android'; Configuration='Development'; Rhi='vulkan' }
        @{ Script='package-android'; Platform='Android'; Configuration='Shipping'; Rhi='gles' }
    ) {
        $arguments = @('-WhatIf', '-DoctorScript', $pass, '-Configuration', $Configuration)
        if ($Rhi) { $arguments += @('-Rhi', $Rhi) }
        $result = Invoke-PackagePreview $Script $arguments
        $result.Code | Should -Be 0
        foreach ($token in @('BuildCookRun', '-project=', "-platform=$Platform", "-clientconfig=$Configuration", '-cook', '-stage', '-package', '-build')) {
            $result.Text | Should -Match ([regex]::Escape($token))
        }
        Test-Path -LiteralPath $generated | Should -BeFalse
    }
    It 'previews RHI overrides differing only in the two booleans and uses separate archives' {
        $vulkan = Invoke-PackagePreview 'package-android' @('-Rhi','vulkan','-WhatIf','-DoctorScript',$pass)
        $gles = Invoke-PackagePreview 'package-android' @('-Rhi','gles','-WhatIf','-DoctorScript',$pass)
        $vulkan.Text | Should -Match 'bSupportsVulkan=True\nbBuildForES31=False'
        $gles.Text | Should -Match 'bSupportsVulkan=False\nbBuildForES31=True'
        $vulkanIni = [regex]::Match($vulkan.Text, '(?s)\[/Script/AndroidRuntimeSettings.AndroidRuntimeSettings\].*?\n\n').Value
        $glesIni = [regex]::Match($gles.Text, '(?s)\[/Script/AndroidRuntimeSettings.AndroidRuntimeSettings\].*?\n\n').Value
        ($vulkanIni -replace 'True|False','BOOL') | Should -Be ($glesIni -replace 'True|False','BOOL')
        $vulkan.Text | Should -Match 'archivedirectory=.*[/\\]vulkan'
        $gles.Text | Should -Match 'archivedirectory=.*[/\\]gles'
        Test-Path -LiteralPath $generated | Should -BeFalse
    }
    It 'requires -Rhi without prompting' {
        $result = Invoke-PackagePreview 'package-android' @('-WhatIf','-DoctorScript',$pass)
        $result.Code | Should -Be 1
        $result.Text | Should -Match '\-Rhi'
        $result.Text | Should -Not -Match 'RunUAT'
    }
    It 'stops <Script> on a failed doctor before UAT' -ForEach @(
        @{ Script='package-windows'; Rhi='' }
        @{ Script='package-android'; Rhi='vulkan' }
        @{ Script='package-android'; Rhi='gles' }
    ) {
        $arguments = @('-WhatIf','-DoctorScript',$fail)
        if ($Rhi) { $arguments += @('-Rhi',$Rhi) }
        $result = Invoke-PackagePreview $Script $arguments
        $result.Code | Should -Be 1
        $result.Text | Should -Match 'Fake doctor FAIL'
        $result.Text | Should -Not -Match 'RunUAT'
    }
}

Describe 'Real smoke packaging' {
    BeforeAll {
        $uat = Join-Path $PSScriptRoot 'fakes/uat.bat'
        function Invoke-RealPackage([string]$Platform = 'Android', [string]$Configuration = 'Development') {
            $output = @(Invoke-SmokePackage -Platform $Platform -Configuration $Configuration -Rhi vulkan -DoctorScript $pass -UatPath $uat 6>&1)
            @{ Code = $output[-1]; Text = ($output | Out-String) }
        }
        function Use-PackageFixture([scriptblock]$Body) {
            if (Test-Path -LiteralPath $generated) { throw "Stale override at test start: $generated" }
            $oldRecord = $env:SMOKE_UAT_RECORD
            $oldExit = $env:SMOKE_UAT_EXIT
            $env:SMOKE_UAT_RECORD = Join-Path $TestDrive ([guid]::NewGuid().ToString())
            $env:SMOKE_UAT_EXIT = '0'
            New-Item -ItemType Directory -Path $env:SMOKE_UAT_RECORD | Out-Null
            try { & $Body } finally {
                if (Test-Path -LiteralPath $generated) { Remove-Item -LiteralPath $generated -Force }
                $env:SMOKE_UAT_RECORD = $oldRecord
                $env:SMOKE_UAT_EXIT = $oldExit
            }
        }
    }
    It 'captures exact Android ini bytes at UAT invocation and cleans up success' {
        Use-PackageFixture {
            Test-Path -LiteralPath (Resolve-PwshPath) | Should -BeTrue
            $result = Invoke-RealPackage
            $result.Code | Should -Be 0
            $expected = [Text.UTF8Encoding]::new($false).GetBytes("[/Script/AndroidRuntimeSettings.AndroidRuntimeSettings]`nbSupportsVulkan=True`nbBuildForES31=False`n")
            $actual = [IO.File]::ReadAllBytes((Join-Path $env:SMOKE_UAT_RECORD 'GeneratedEngine.ini'))
            [Convert]::ToBase64String($actual) | Should -Be ([Convert]::ToBase64String($expected))
            Get-Content (Join-Path $env:SMOKE_UAT_RECORD 'arguments.txt') -Raw | Should -Match '\-platform=Android'
            Test-Path -LiteralPath $generated | Should -BeFalse
        }
    }
    It 'preserves UAT exit 7 and cleans up the Android ini' {
        Use-PackageFixture {
            $env:SMOKE_UAT_EXIT = '7'
            (Invoke-RealPackage).Code | Should -Be 7
            Test-Path (Join-Path $env:SMOKE_UAT_RECORD 'GeneratedEngine.ini') | Should -BeTrue
            Test-Path -LiteralPath $generated | Should -BeFalse
        }
    }
    It 'never creates an ini for Windows packaging' {
        Use-PackageFixture {
            # no-ini.txt is written by the fake UAT itself, so it records the state at
            # invocation time rather than after the fact.
            (Invoke-RealPackage -Platform Win64).Code | Should -Be 0
            Test-Path (Join-Path $env:SMOKE_UAT_RECORD 'no-ini.txt') | Should -BeTrue
            Test-Path (Join-Path $env:SMOKE_UAT_RECORD 'GeneratedEngine.ini') | Should -BeFalse
            Test-Path -LiteralPath $generated | Should -BeFalse
        }
    }
    It 'preserves a pre-existing ini and never invokes UAT' {
        Use-PackageFixture {
            [IO.File]::WriteAllText($generated, 'existing override')
            (Invoke-RealPackage).Code | Should -Not -Be 0
            [IO.File]::ReadAllText($generated) | Should -BeExactly 'existing override'
            @(Get-ChildItem $env:SMOKE_UAT_RECORD).Count | Should -Be 0
        }
    }
    It 'reports a cleanup failure and preserves an earlier UAT failure' {
        # The fake UAT deletes the override itself, so the script's own Remove-Item fails on a
        # missing file. That reaches the same catch as a denied or locked delete without holding a
        # file handle across a mock, which leaks the handle into a finalizer when the mock throws.
        Use-PackageFixture {
            $env:SMOKE_UAT_DELETE_INI = '1'
            try {
                foreach ($code in @('0', '7')) {
                    $env:SMOKE_UAT_EXIT = $code
                    $result = Invoke-RealPackage
                    # 2 means packaging succeeded but the override could not be removed. A run that
                    # already failed keeps its own code, because that is the more useful one.
                    $result.Code | Should -Be $(if ($code -eq '0') { 2 } else { 7 })
                    $result.Text | Should -Match ([regex]::Escape($generated))
                    $result.Text | Should -Match 'Cleanup failed'
                    Test-Path (Join-Path $env:SMOKE_UAT_RECORD 'GeneratedEngine.ini') | Should -BeTrue
                }
            } finally { $env:SMOKE_UAT_DELETE_INI = $null }
        }
    }
    It 'propagates arbitrary UAT exit codes 3 then 0' {
        Use-PackageFixture {
            foreach ($code in @(3, 0)) {
                $env:SMOKE_UAT_EXIT = "$code"
                (Invoke-RealPackage).Code | Should -Be $code
                Test-Path (Join-Path $env:SMOKE_UAT_RECORD 'arguments.txt') | Should -BeTrue
                Test-Path -LiteralPath $generated | Should -BeFalse
            }
        }
    }
    It 'refuses an archive containing ampersand without invoking UAT' {
        Use-PackageFixture {
            $result = Invoke-RealPackage -Configuration 'Dev&Unsafe'
            $result.Code | Should -Not -Be 0
            $result.Text | Should -Match "metacharacter '&'"
            $result.Text | Should -Match 'Dev&Unsafe'
            @(Get-ChildItem $env:SMOKE_UAT_RECORD).Count | Should -Be 0
            Test-Path -LiteralPath $generated | Should -BeFalse
        }
    }
}
