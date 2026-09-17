BeforeAll {
    $root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
    $pass = Join-Path $PSScriptRoot 'fakes/doctor-pass.ps1'
    $fail = Join-Path $PSScriptRoot 'fakes/doctor-fail.ps1'
    $generated = Join-Path $root 'runtime/UnRealDash/Config/GeneratedEngine.ini'
    function Invoke-PackagePreview($Script, $Arguments) {
        $lines = & (Join-Path $PSHOME 'pwsh.exe') -NoProfile -File (Join-Path $root "scripts/$Script.ps1") @Arguments 2>&1
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
