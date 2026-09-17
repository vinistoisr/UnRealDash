BeforeAll {
    . (Join-Path $PSScriptRoot '../SignalCoreLayering.ps1')
}
Describe 'PLAN 4.4 DashboardSpec layering' {
    It 'allows the lower library dependency and registration exception only' {
        $source = Join-Path $TestDrive 'Source'
        foreach ($folder in @('DashboardSpec/Private', 'Game', 'UnRealDashCore/Public', 'UnRealDashCore/Private')) {
            New-Item -ItemType Directory -Path (Join-Path $source $folder) -Force | Out-Null
        }
        Set-Content (Join-Path $source 'DashboardSpec/Private/DashboardSpecModule.cpp') '#include "Modules/ModuleManager.h"'
        Set-Content (Join-Path $source 'DashboardSpec/Private/portable.cpp') 'signal_core::Sample sample;'
        Set-Content (Join-Path $source 'UnRealDashCore/Private/loader.cpp') '#include "dashboard_spec/PackageReader.h"'
        @(Get-SignalCoreLayeringViolation $source).Count | Should -Be 0
        @(Get-DashboardSpecLayeringViolation $source).Count | Should -Be 0
        Set-Content (Join-Path $source 'DashboardSpec/Private/portable.cpp') '#include "CoreMinimal.h"'
        Set-Content (Join-Path $source 'Game/game.cpp') 'dashboard_spec::Document document;'
        Set-Content (Join-Path $source 'UnRealDashCore/Public/leak.h') '#include "dashboard_spec/Document.h"'
        @(Get-DashboardSpecLayeringViolation $source).Count | Should -Be 3
    }
    It 'accepts the current source tree and is wired into doctor' {
        @(Get-DashboardSpecLayeringViolation (Join-Path $PSScriptRoot '../../runtime/UnRealDash/Source')).Count | Should -Be 0
        Get-Content (Join-Path $PSScriptRoot '../doctor.ps1') -Raw | Should -Match 'Get-DashboardSpecLayeringViolation'
    }
    It 'keeps one authoritative copy and pinned non-UFS schemas' {
        $root = Join-Path $PSScriptRoot '../..'
        Test-Path (Join-Path $root 'packages/dashboard-spec/src') | Should -BeFalse
        Test-Path (Join-Path $root 'packages/dashboard-spec/include') | Should -BeFalse
        $rules = Get-Content (Join-Path $root 'runtime/UnRealDash/Source/DashboardSpec/DashboardSpec.Build.cs') -Raw
        $rules | Should -Match '\$\(ProjectDir\)/Schema/'
        $rules | Should -Match 'StagedFileType.NonUFS'
        @(Get-ChildItem (Join-Path $root 'packages/dashboard-spec/schema') -Filter '*.schema.json').Count | Should -Be 5
    }
}
