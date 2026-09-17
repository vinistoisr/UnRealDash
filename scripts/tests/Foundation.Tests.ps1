BeforeDiscovery {
    $cases = @(
        @{ Script = 'build'; Target = 'win64'; Profile = 'workstation'; Platform = 'Win64' }
        @{ Script = 'build'; Target = 'android'; Profile = 'android'; Platform = 'Android' }
        @{ Script = 'build'; Target = 'linux'; Profile = 'linux'; Platform = 'Linux' }
        @{ Script = 'package-windows'; Target = ''; Profile = 'workstation'; Platform = 'Win64' }
        @{ Script = 'package-android'; Target = ''; Profile = 'android'; Platform = 'Android' }
        @{ Script = 'package-linux'; Target = ''; Profile = 'linux'; Platform = 'Linux' }
        @{ Script = 'deploy-deck'; Target = ''; Profile = 'device'; Platform = 'Android' }
        @{ Script = 'capture-metrics'; Target = 'windows'; Profile = 'workstation'; Platform = 'Win64' }
        @{ Script = 'capture-metrics'; Target = 'device'; Profile = 'device'; Platform = 'Android' }
        @{ Script = 'capture-metrics'; Target = 'soak'; Profile = 'workstation'; Platform = 'Win64' }
    )
}

BeforeAll {
    $root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
    $scripts = Join-Path $root 'scripts'
    $pins = Get-Content (Join-Path $scripts 'pins.json') -Raw | ConvertFrom-Json
    $pwsh = Join-Path $PSHOME 'pwsh.exe'
    # These memberships come from PLAN.md 0.7, independently of pins.json.
    $workstationRows = @('C: free space', 'Git LFS', 'cmake', 'ninja', 'Visual Studio', 'MSVC toolset', 'Windows SDK', 'Unreal Engine')
    $planRows = @{
        workstation = $workstationRows
        # Engine target-platform support is a separate optional download, so each target profile
        # carries its own row for it rather than the workstation profile demanding both.
        # 'Project JDK' is a PLAN 0.7 amendment: JAVA_HOME is not the JDK gradle runs on, so the
        # JDK the project hands gradle is checked separately from the one UnrealBuildTool requires.
        android = $workstationRows + @('Engine Android support', 'Android Studio', 'Android NDK', 'JDK', 'Project JDK', 'Android SDK')
        linux = $workstationRows + @('Engine Linux support', 'Linux cross toolchain', 'LINUX_MULTIARCH_ROOT')
        device = $workstationRows + @('ADB reachability', 'Device free storage')
    }
    function Invoke-Child([string]$Script, [string[]]$Arguments, [hashtable]$Environment = @{}) {
        $saved = @{}
        try {
            foreach ($name in $Environment.Keys) {
                $saved[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
                [Environment]::SetEnvironmentVariable($name, $Environment[$name], 'Process')
            }
            $output = & $pwsh -NoProfile -File (Join-Path $scripts $Script) @Arguments 2>&1
            $code = $LASTEXITCODE
            [pscustomobject]@{ Code = $code; Text = ($output | Out-String -Width 240) }
        } finally {
            foreach ($name in $saved.Keys) { [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process') }
        }
    }
    function Get-PrintedRows($Result) {
        foreach ($line in ($Result.Text -split '\r?\n')) {
            if ($line -match '^(\S.*?)\s{2,}.*\s(PASS|FAIL)\s*$') {
                [pscustomobject]@{ Component = $Matches[1]; Status = $Matches[2] }
            }
        }
    }
    function Assert-Row($Result, [string]$Component, [string]$Status, [int]$ExitCode) {
        $rows = @(Get-PrintedRows $Result | Where-Object { $_.Component -eq $Component })
        $rows.Count | Should -Be 1 -Because $Result.Text
        $rows[0].Status | Should -Be $Status -Because $Result.Text
        $Result.Code | Should -Be $ExitCode -Because $Result.Text
    }
    function New-FixturePins([string[]]$Ids) {
        $data = $pins | ConvertTo-Json -Depth 12 | ConvertFrom-Json
        $data.rows = @($data.rows | Where-Object { $_.id -in $Ids })
        return $data
    }
    function Save-FixturePins($Data) {
        $path = Join-Path $TestDrive 'fixture-pins.json'
        $Data | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $path
        return $path
    }
    # The Android rows read the installed engine's own Android_SDK.json, so a fixture engine
    # is what lets a test choose the versions it then creates or withholds.
    function New-FixtureEngine($Data, [string]$Name, [hashtable]$Manifest) {
        $root = Join-Path $TestDrive $Name
        $config = Join-Path $root 'Engine/Config/Android'
        [void](New-Item $config -ItemType Directory -Force)
        [void](New-Item (Join-Path $root 'Engine/Build') -ItemType Directory -Force)
        Set-Content (Join-Path $root 'Engine/Build/Build.version') '{"MajorVersion":5,"MinorVersion":8,"PatchVersion":2}'
        Set-Content (Join-Path $root 'Engine/Build/InstalledBuild.txt') ''
        $Manifest | ConvertTo-Json | Set-Content (Join-Path $config 'Android_SDK.json')
        $Data.engine.root = $root
        return $root
    }
    function Write-Wrapper([string]$Directory, [string]$Name, [string]$Body) {
        [void](New-Item -ItemType Directory -Path $Directory -Force)
        Set-Content -LiteralPath (Join-Path $Directory $Name) -Value $Body
    }
}

Describe 'Doctor profiles and seams' {
    It '<Profile> prints exactly the PLAN 0.7 row set' -ForEach @(
        @{ Profile = 'workstation' }, @{ Profile = 'android' }, @{ Profile = 'linux' }, @{ Profile = 'device' }
    ) {
        $result = Invoke-Child 'doctor.ps1' @('-Profile', $Profile)
        $result.Text | Should -Match "Doctor profile: $Profile"
        $expected = @($planRows[$Profile] | Sort-Object)
        $actual = @(Get-PrintedRows $result | ForEach-Object { $_.Component } | Sort-Object)
        ($actual -join '|') | Should -Be ($expected -join '|')
        $result.Code | Should -BeIn @(0, 1)
    }

    It 'detects NDK membership added to workstation contrary to PLAN 0.7' {
        $data = $pins | ConvertTo-Json -Depth 12 | ConvertFrom-Json
        ($data.rows | Where-Object { $_.id -eq 'ndk' }).profiles += 'workstation'
        $result = Invoke-Child 'doctor.ps1' @('-Profile', 'workstation', '-PinsFile', (Save-FixturePins $data))
        $actual = @(Get-PrintedRows $result | ForEach-Object { $_.Component } | Sort-Object)
        $actual | Should -Contain 'Android NDK'
        ($actual -join '|') | Should -Not -Be (($planRows.workstation | Sort-Object) -join '|')
        ($actual -join '|') | Should -Be ((($planRows.workstation + 'Android NDK') | Sort-Object) -join '|')
    }

    It 'SearchPath without cmake fails workstation and identifies cmake' {
        $empty = New-Item (Join-Path $TestDrive 'empty-path') -ItemType Directory
        $result = Invoke-Child 'doctor.ps1' @('-Profile', 'workstation', '-SearchPath', $empty.FullName)
        $result.Code | Should -Be 1
        $result.Text | Should -Match 'cmake not found on search PATH'
        $result.Text | Should -Match '(?m)^cmake\s+.*FAIL'
    }

    It 'PreInstall passes on this machine' {
        $result = Invoke-Child 'doctor.ps1' @('-PreInstall')
        $result.Code | Should -Be 0 -Because $result.Text
    }

    It 'PreInstall checks only capacity and installer prerequisites' {
        $result = Invoke-Child 'doctor.ps1' @('-Profile', 'android', '-PreInstall')
        $expected = @('C: free space', 'git', 'winget', 'PowerShell', 'Signed-in shell', 'User PATH writable' | Sort-Object)
        $actual = @(Get-PrintedRows $result | ForEach-Object { $_.Component } | Sort-Object)
        ($actual -join '|') | Should -Be ($expected -join '|')
        $result.Text | Should -Not -Match 'Unreal Engine|Android NDK|Android SDK|JDK|cmake|ninja'
    }

    It 'PinsFile controls membership and expected matching without changing defaults' {
        $custom = Join-Path $TestDrive 'pins.json'
        $data = @{ rows = @(
            @{ component = 'Selected git'; expected = 'test version'; detect = 'command'; profiles = @('workstation'); command = 'git'; arguments = @('--version'); pattern = '^git version' }
            @{ component = 'Excluded'; expected = 'missing'; detect = 'command'; profiles = @('android'); command = 'does-not-exist'; arguments = @(); pattern = '.' }
        ) }
        $data | ConvertTo-Json -Depth 8 | Set-Content $custom
        $result = Invoke-Child 'doctor.ps1' @('-PinsFile', $custom)
        $result.Code | Should -Be 0
        $result.Text | Should -Match 'Selected git'
        $result.Text | Should -Not -Match 'Excluded'
        $data.rows[0].pattern = '^impossible-version$'
        $data | ConvertTo-Json -Depth 8 | Set-Content $custom
        (Invoke-Child 'doctor.ps1' @('-PinsFile', $custom)).Code | Should -Be 1
    }

    It 'fails closed for missing, invalid and empty pin files' {
        (Invoke-Child 'doctor.ps1' @('-PinsFile', (Join-Path $TestDrive 'absent.json'))).Code | Should -Be 1
        $custom = Join-Path $TestDrive 'invalid.json'
        'not json' | Set-Content $custom
        (Invoke-Child 'doctor.ps1' @('-PinsFile', $custom)).Code | Should -Be 1
        '{"rows":[]}' | Set-Content $custom
        (Invoke-Child 'doctor.ps1' @('-PinsFile', $custom)).Code | Should -Be 1
    }
}

Describe 'Deterministic doctor transitions' {
    It 'workstation cmake changes from FAIL to PASS with a cmd wrapper on SearchPath' {
        $directory = Join-Path $TestDrive 'tools with spaces'
        [void](New-Item $directory -ItemType Directory)
        $file = Save-FixturePins (New-FixturePins @('cmake'))
        $arguments = @('-Profile', 'workstation', '-PinsFile', $file, '-SearchPath', $directory)
        Assert-Row (Invoke-Child 'doctor.ps1' $arguments) 'cmake' 'FAIL' 1
        $wrapper = '@echo off', 'if not "%~1"=="--version" exit /b 9', 'echo cmake version 4.4.3'
        Write-Wrapper $directory 'cmake.cmd' ($wrapper -join "`r`n")
        Assert-Row (Invoke-Child 'doctor.ps1' $arguments) 'cmake' 'PASS' 0
    }

    It 'android NDK changes from FAIL to PASS when the engine manifest version appears' {
        $data = New-FixturePins @('ndk')
        [void](New-FixtureEngine $data 'ndk-engine' @{ MainVersion = 'r27c'; ndk = '27.2.12479018'; platforms = 'android-36'; 'build-tools' = '36.0.0'; cmake = '3.22.1' })
        $sdk = Join-Path $TestDrive 'android-sdk'
        $file = Save-FixturePins $data
        $environment = @{ ANDROID_HOME = $sdk; ANDROID_SDK_ROOT = (Join-Path $TestDrive 'unused-sdk'); NDKROOT = $null }
        $arguments = @('-Profile', 'android', '-PinsFile', $file, '-SearchPath', $TestDrive)
        Assert-Row (Invoke-Child 'doctor.ps1' $arguments $environment) 'Android NDK' 'FAIL' 1
        $ndk = Join-Path $sdk 'ndk/27.2.12479018'
        [void](New-Item $ndk -ItemType Directory -Force)
        Set-Content (Join-Path $ndk 'source.properties') 'Pkg.Revision = 27.2.12479018'
        Assert-Row (Invoke-Child 'doctor.ps1' $arguments $environment) 'Android NDK' 'PASS' 0
    }

    It 'linux root and toolchain change from FAIL to PASS using the internal version marker' {
        $data = New-FixturePins @('linux-toolchain', 'linux-root')
        $file = Save-FixturePins $data
        $arguments = @('-Profile', 'linux', '-PinsFile', $file, '-SearchPath', $TestDrive)
        $missing = Invoke-Child 'doctor.ps1' $arguments @{ LINUX_MULTIARCH_ROOT = $null }
        Assert-Row $missing 'Linux cross toolchain' 'FAIL' 1
        Assert-Row $missing 'LINUX_MULTIARCH_ROOT' 'FAIL' 1
        $toolchain = Join-Path $TestDrive 'arbitrary-toolchain-location'
        $version = ($data.rows | Where-Object { $_.id -eq 'linux-toolchain' }).version
        Write-Wrapper (Join-Path $toolchain 'x86_64-unknown-linux-gnu/bin') 'clang++.cmd' "@echo off`r`necho clang version $version"
        $present = Invoke-Child 'doctor.ps1' $arguments @{ LINUX_MULTIARCH_ROOT = $toolchain }
        Assert-Row $present 'Linux cross toolchain' 'PASS' 0
        Assert-Row $present 'LINUX_MULTIARCH_ROOT' 'PASS' 0
    }

    It 'rejects clang 20.1.80 against the 20.1.8 pin' {
        $data = New-FixturePins @('linux-toolchain')
        $file = Save-FixturePins $data
        $toolchain = Join-Path $TestDrive 'different-location'
        Write-Wrapper (Join-Path $toolchain 'x86_64-unknown-linux-gnu/bin') 'clang++.cmd' "@echo off`r`necho clang version 20.1.80"
        $result = Invoke-Child 'doctor.ps1' @('-Profile', 'linux', '-PinsFile', $file) @{ LINUX_MULTIARCH_ROOT = $toolchain }
        Assert-Row $result 'Linux cross toolchain' 'FAIL' 1
        $result.Text | Should -Match 'clang version 20\.1\.80'
    }

    It 'device reachability and storage change from FAIL to PASS with the configured fake serial' {
        $data = New-FixturePins @('adb', 'device-storage')
        $data.device.adb_address = '192.0.2.17:5555'
        $file = Save-FixturePins $data
        $directory = Join-Path $TestDrive 'adb-tools'
        [void](New-Item $directory -ItemType Directory)
        $arguments = @('-Profile', 'device', '-PinsFile', $file, '-SearchPath', $directory)
        $missing = Invoke-Child 'doctor.ps1' $arguments
        Assert-Row $missing 'ADB reachability' 'FAIL' 1
        Assert-Row $missing 'Device free storage' 'FAIL' 1
        $wrapper = @'
if ($args[0] -ne '-s' -or $args[1] -ne 'SERIAL') { exit 9 }
if ($args[2] -eq 'get-state') { Write-Output 'device'; exit 0 }
if (($args[2..5] -join ' ') -eq 'shell df -k /data') {
    Write-Output 'Filesystem 1K-blocks Used Available Use% Mounted on'
    Write-Output '/dev/fake 100000 40000 60000 40% /data'
    exit 0
}
exit 9
'@
        Write-Wrapper $directory 'adb.ps1' ($wrapper.Replace('SERIAL', $data.device.adb_address))
        $present = Invoke-Child 'doctor.ps1' $arguments
        Assert-Row $present 'ADB reachability' 'PASS' 0
        Assert-Row $present 'Device free storage' 'PASS' 0
        $present.Text | Should -Match '60000 KiB available on /data'
    }

    It '<Mode> capacity changes from PASS to FAIL with a 999999 GB minimum' -ForEach @(
        @{ Mode = 'workstation'; Id = 'disk'; Arguments = @('-Profile', 'workstation') }
        @{ Mode = 'preinstall'; Id = 'pre-disk'; Arguments = @('-PreInstall') }
    ) {
        $data = New-FixturePins @($Id)
        $data.rows[0].minimum = 0
        $file = Save-FixturePins $data
        $invokeArguments = $Arguments + @('-PinsFile', $file, '-SearchPath', $TestDrive)
        Assert-Row (Invoke-Child 'doctor.ps1' $invokeArguments) 'C: free space' 'PASS' 0
        $data.rows[0].minimum = 999999
        $data.rows[0].expected = '>= 999999 GB'
        [void](Save-FixturePins $data)
        Assert-Row (Invoke-Child 'doctor.ps1' $invokeArguments) 'C: free space' 'FAIL' 1
    }

    It 'the project JDK row enforces both ends of the range gradle supports' {
        # UnRealDash_UPL.xml writes this JDK into gradle.properties as org.gradle.java.home. It is a
        # separate row from JDK because JAVA_HOME is not what gradle ends up running: UnrealBuildTool
        # replaces it with Android Studio's bundled JDK when its SDK layout check is not satisfied.
        $data = New-FixturePins @('project-jdk')
        $file = Save-FixturePins $data
        $floor = $data.jdk.minimum_major
        $ceiling = $data.jdk.maximum_major
        $jdkHome = Join-Path $TestDrive 'project-jdk-home'
        $arguments = @('-Profile', 'android', '-PinsFile', $file)
        # Not set at all is a failure that names the variable. The row falls back to the stored
        # user value, which is set on a configured machine, so the unset case is driven through a
        # pins file naming a variable that cannot exist rather than by clearing the real one.
        $absentData = New-FixturePins @('project-jdk')
        $absentData.rows[0].variable = 'UNREALDASH_JAVA_HOME_ABSENT_FIXTURE'
        $absent = Invoke-Child 'doctor.ps1' @('-Profile', 'android', '-PinsFile', (Save-FixturePins $absentData))
        Assert-Row $absent 'Project JDK' 'FAIL' 1
        $absent.Text | Should -Match 'UNREALDASH_JAVA_HOME_ABSENT_FIXTURE'
        $file = Save-FixturePins $data
        foreach ($case in @(
            @{ Version = "$floor.0.1"; Status = 'PASS'; Code = 0 }
            @{ Version = "$ceiling.0.1"; Status = 'PASS'; Code = 0 }
            @{ Version = '11.0.2'; Status = 'FAIL'; Code = 1 }
            @{ Version = "$($ceiling + 1).0.1"; Status = 'FAIL'; Code = 1 }
        )) {
            Write-Wrapper (Join-Path $jdkHome 'bin') 'java.ps1' "Write-Output 'openjdk version `"$($case.Version)`"'"
            $result = Invoke-Child 'doctor.ps1' $arguments @{ UNREALDASH_JAVA_HOME = $jdkHome }
            Assert-Row $result 'Project JDK' $case.Status $case.Code
            $result.Text | Should -Match ([regex]::Escape($case.Version))
        }
    }

    It 'JDK prefers JAVA_HOME and enforces the major-version floor' {
        # A floor only. UnrealBuildTool refuses to build Android below 17, but it overwrites
        # JAVA_HOME before gradle runs, so the range gradle needs belongs to the Project JDK row.
        $data = New-FixturePins @('jdk')
        $file = Save-FixturePins $data
        $floor = $data.rows[0].minimum_major
        $jdkHome = Join-Path $TestDrive 'jdk-home'
        $directory = Join-Path $TestDrive 'java-path'
        Write-Wrapper $directory 'java.cmd' "@echo off`r`necho openjdk version `"$floor.0.1`""
        Write-Wrapper (Join-Path $jdkHome 'bin') 'java.ps1' "Write-Output 'openjdk version `"$floor.0.3`"'"
        $arguments = @('-Profile', 'android', '-PinsFile', $file, '-SearchPath', $directory)
        $matching = Invoke-Child 'doctor.ps1' $arguments @{ JAVA_HOME = $jdkHome }
        Assert-Row $matching 'JDK' 'PASS' 0
        $matching.Text | Should -Match 'JAVA_HOME'
        # A JDK below the floor fails even when the search PATH holds an acceptable one.
        Write-Wrapper (Join-Path $jdkHome 'bin') 'java.ps1' 'Write-Output ''openjdk version "11.0.2"'''
        $mismatch = Invoke-Child 'doctor.ps1' $arguments @{ JAVA_HOME = $jdkHome }
        Assert-Row $mismatch 'JDK' 'FAIL' 1
        $mismatch.Text | Should -Match 'JAVA_HOME'
        $mismatch.Text | Should -Match '11\.0\.2'
        # A JDK above the gradle ceiling is fine here: this row is not what gradle runs on.
        Write-Wrapper (Join-Path $jdkHome 'bin') 'java.ps1' 'Write-Output ''openjdk version "25.0.3"'''
        Assert-Row (Invoke-Child 'doctor.ps1' $arguments @{ JAVA_HOME = $jdkHome }) 'JDK' 'PASS' 0
        $fallback = Invoke-Child 'doctor.ps1' $arguments @{ JAVA_HOME = $null }
        Assert-Row $fallback 'JDK' 'PASS' 0
        $fallback.Text | Should -Match 'search PATH'
    }

    It 'engine platform support is decided by UnrealBuildTool, not by the file layout' -ForEach @(
        @{ Platform = 'Android'; Profile = 'android'; Row = 'Engine Android support' }
        @{ Platform = 'Linux'; Profile = 'linux'; Row = 'Engine Linux support' }
    ) {
        # A launcher install ships editor-side target-platform modules for platforms it cannot build,
        # so the probe asks UnrealBuildTool and the test drives its answer through a fake Build.bat.
        $data = New-FixturePins @("engine-$($Platform.ToLower())")
        $root = Join-Path $TestDrive "engine-$Platform"
        $batchDirectory = Join-Path $root 'Engine/Build/BatchFiles'
        [void](New-Item $batchDirectory -ItemType Directory -Force)
        $data.engine.root = $root
        $file = Save-FixturePins $data
        Write-Wrapper $batchDirectory 'Build.cmd' "@echo off`r`necho ##PlatformValidate: $Platform INVALID"
        Copy-Item (Join-Path $batchDirectory 'Build.cmd') (Join-Path $batchDirectory 'Build.bat')
        $invalid = Invoke-Child 'doctor.ps1' @('-Profile', $Profile, '-PinsFile', $file)
        Assert-Row $invalid $Row 'FAIL' 1
        $invalid.Text | Should -Match 'Epic Games Launcher'
        Set-Content (Join-Path $batchDirectory 'Build.bat') "@echo off`r`necho ##PlatformValidate: $Platform VALID"
        Assert-Row (Invoke-Child 'doctor.ps1' @('-Profile', $Profile, '-PinsFile', $file)) $Row 'PASS' 0
    }


    It 'Windows SDK kits_root override changes from FAIL to PASS when pinned Include exists' {
        $data = New-FixturePins @('windows-sdk')
        $sdk = Join-Path $TestDrive 'custom-kits'
        $data.rows[0] | Add-Member -NotePropertyName kits_root -NotePropertyValue $sdk -Force
        $file = Save-FixturePins $data
        $arguments = @('-Profile', 'workstation', '-PinsFile', $file)
        Assert-Row (Invoke-Child 'doctor.ps1' $arguments) 'Windows SDK' 'FAIL' 1
        [void](New-Item (Join-Path $sdk "Include/$($data.rows[0].version).0") -ItemType Directory -Force)
        $result = Invoke-Child 'doctor.ps1' $arguments
        Assert-Row $result 'Windows SDK' 'PASS' 0
        $result.Text | Should -Match 'custom-kits'
    }

    It 'Android SDK requires the platform, build-tools and cmake the engine manifest names' {
        $data = New-FixturePins @('android-sdk')
        [void](New-FixtureEngine $data 'sdk-engine' @{ MainVersion = 'r27c'; ndk = '27.2.12479018'; platforms = 'android-36'; 'build-tools' = '36.0.0'; cmake = '3.22.1' })
        $sdk = Join-Path $TestDrive 'sdk-artifacts'
        $arguments = @('-Profile', 'android', '-PinsFile', (Save-FixturePins $data))
        $environment = @{ ANDROID_HOME = $sdk; ANDROID_SDK_ROOT = (Join-Path $TestDrive 'absent-sdk') }
        $platform = Join-Path $sdk 'platforms/android-36'
        [void](New-Item $platform -ItemType Directory -Force)
        Set-Content (Join-Path $platform 'android.jar') 'fixture'
        Assert-Row (Invoke-Child 'doctor.ps1' $arguments $environment) 'Android SDK' 'FAIL' 1
        [void](New-Item (Join-Path $sdk 'build-tools/36.0.0') -ItemType Directory -Force)
        Assert-Row (Invoke-Child 'doctor.ps1' $arguments $environment) 'Android SDK' 'FAIL' 1
        [void](New-Item (Join-Path $sdk 'cmake/3.22.1') -ItemType Directory -Force)
        Assert-Row (Invoke-Child 'doctor.ps1' $arguments $environment) 'Android SDK' 'PASS' 0
        # A platform the manifest does not name must not satisfy the row.
        $other = Join-Path $sdk 'platforms/android-35'
        [void](New-Item $other -ItemType Directory -Force)
        Set-Content (Join-Path $other 'android.jar') 'fixture'
        Remove-Item $platform -Recurse -Force
        Assert-Row (Invoke-Child 'doctor.ps1' $arguments $environment) 'Android SDK' 'FAIL' 1
    }

    It 'SDK and NDK use the shared <Source> root fallback' -ForEach @(
        @{ Source = 'ANDROID_SDK_ROOT' }, @{ Source = 'LOCALAPPDATA' }
    ) {
        $data = New-FixturePins @('android-sdk', 'ndk')
        [void](New-FixtureEngine $data "fallback-engine-$Source" @{ MainVersion = 'r27c'; ndk = '27.2.12479018'; platforms = 'android-36'; 'build-tools' = '36.0.0'; cmake = '3.22.1' })
        $sdk = Join-Path $TestDrive 'fallback/Android/Sdk'
        $platform = Join-Path $sdk 'platforms/android-36'
        $ndk = Join-Path $sdk 'ndk/27.2.12479018'
        foreach ($directory in @($platform, $ndk, (Join-Path $sdk 'build-tools/36.0.0'), (Join-Path $sdk 'cmake/3.22.1'))) {
            [void](New-Item $directory -ItemType Directory -Force)
        }
        Set-Content (Join-Path $platform 'android.jar') 'fixture'
        Set-Content (Join-Path $ndk 'source.properties') 'Pkg.Revision = 27.2.12479018'
        $environment = @{ ANDROID_HOME = $null; ANDROID_SDK_ROOT = $null; NDKROOT = $null }
        if ($Source -eq 'ANDROID_SDK_ROOT') { $environment.ANDROID_SDK_ROOT = $sdk }
        else { $environment.LOCALAPPDATA = Join-Path $TestDrive 'fallback' }
        $result = Invoke-Child 'doctor.ps1' @('-Profile', 'android', '-PinsFile', (Save-FixturePins $data)) $environment
        Assert-Row $result 'Android SDK' 'PASS' 0
        Assert-Row $result 'Android NDK' 'PASS' 0
    }
}

Describe 'Skeleton doctor gates' {
    It '<Script> <Target> previews <Platform> only after a passing <Profile> doctor' -ForEach $cases {
        $arguments = @('-WhatIf', '-DoctorScript', (Join-Path $PSScriptRoot 'fakes/doctor-pass.ps1'))
        if ($Script -eq 'package-android') { $arguments += @('-Rhi', 'vulkan') }
        if ($Target) { $arguments += @('-Target', $Target) }
        $result = Invoke-Child "$Script.ps1" $arguments
        $result.Code | Should -Be 0
        $result.Text | Should -Match "Doctor profile: $Profile"
        $result.Text | Should -Match "Fake doctor PASS: $Profile"
        $project = Join-Path $root 'runtime/UnRealDash/UnRealDash.uproject'
        if ($Script -eq 'build') {
            # build.ps1 runs a module build, so its preview is the Build.bat line it executes.
            $batch = Join-Path $pins.engine.root 'Engine/Build/BatchFiles/Build.bat'
            $line = '& "{0}" UnRealDash {1} Development -project="{2}" -waitmutex' -f $batch, $Platform, $project
            $marker = 'Build.bat'
        }
        else {
            $uat = Join-Path $pins.engine.root 'Engine/Build/BatchFiles/RunUAT.bat'
            $line = '& "{0}" BuildCookRun -project="{1}" -noP4 -platform={2} -clientconfig=Development -build -cook -stage -pak -archive' -f $uat, $project, $Platform
            $marker = 'RunUAT'
        }
        $result.Text | Should -Match ([regex]::Escape($line))
        $result.Text.IndexOf('Fake doctor PASS') | Should -BeLessThan $result.Text.IndexOf($marker)
    }

    It '<Script> <Target> stops without UAT when its doctor fails' -ForEach $cases {
        $arguments = @('-WhatIf', '-DoctorScript', (Join-Path $PSScriptRoot 'fakes/doctor-fail.ps1'))
        if ($Script -eq 'package-android') { $arguments += @('-Rhi', 'vulkan') }
        if ($Target) { $arguments += @('-Target', $Target) }
        $result = Invoke-Child "$Script.ps1" $arguments
        $result.Code | Should -Be 1
        $result.Text | Should -Match "Fake doctor FAIL: $Profile"
        $result.Text | Should -Not -Match 'RunUAT'
        $result.Text | Should -Not -Match 'Build\.bat'
    }

    It 'build.ps1 previews the module build it would run, not a packaging command' {
        # Chunk 06 gave build.ps1 a real execution path. The preview must show that command,
        # so what -WhatIf prints is what a run without -WhatIf executes.
        $result = Invoke-Child 'build.ps1' @('-WhatIf', '-DoctorScript', (Join-Path $PSScriptRoot 'fakes/doctor-pass.ps1'))
        $result.Code | Should -Be 0
        $result.Text | Should -Match 'Build\.bat'
        $result.Text | Should -Match 'UnRealDash Win64 Development'
        $result.Text | Should -Not -Match 'RunUAT'
        $result.Text | Should -Not -Match 'BuildCookRun'
        $editor = Invoke-Child 'build.ps1' @('-Target', 'editor', '-WhatIf', '-DoctorScript', (Join-Path $PSScriptRoot 'fakes/doctor-pass.ps1'))
        $editor.Text | Should -Match 'UnRealDashEditor Win64 Development'
    }

    It 'fails closed when the doctor file is missing' {
        $result = Invoke-Child 'build.ps1' @('-WhatIf', '-DoctorScript', (Join-Path $TestDrive 'missing.ps1'))
        $result.Code | Should -Be 1
        $result.Text | Should -Not -Match 'RunUAT'
    }
}

Describe 'Repository foundation' {
    It 'has each specified directory README and root policy file' {
        foreach ($directory in @('packages/signal-core', 'packages/dashboard-spec', 'runtime/UnRealDash', 'connectors', 'tools', 'tests/fixtures', 'examples', 'docs/reports', 'scripts')) {
            (Get-Content (Join-Path $root "$directory/README.md") -Raw).Length | Should -BeGreaterThan 50
        }
        foreach ($file in @('.editorconfig', '.clang-format', '.clang-tidy', 'CONTRIBUTING.md')) {
            Test-Path (Join-Path $root $file) | Should -BeTrue
        }
    }

    It 'tracks every required binary extension with LFS and normalizes text' {
        $attributes = Get-Content (Join-Path $root '.gitattributes')
        foreach ($extension in @('uasset', 'umap', 'png', 'jpg', 'fbx', 'wav', 'ttf', 'exr', 'tga')) {
            $attributes | Should -Contain "*.$extension filter=lfs diff=lfs merge=lfs -text"
        }
        $attributes | Should -Contain '* text=auto eol=lf'
        $attributes | Should -Contain '*.ps1 text eol=crlf'
        $attributes | Should -Contain '*.cmd text eol=crlf'
    }

    It 'contains a real 16 by 16 PNG probe' {
        $bytes = [IO.File]::ReadAllBytes((Join-Path $root 'tests/fixtures/images/lfs-probe.png'))
        [Convert]::ToHexString($bytes[0..7]) | Should -Be '89504E470D0A1A0A'
        [Convert]::ToHexString($bytes[16..23]) | Should -Be '0000001000000010'
    }

    It 'provides provisional licenses and links them' {
        $license = Get-Content (Join-Path $root 'LICENSE') -Raw
        $license | Should -Match '^Provisional'
        $license | Should -Match 'Copyright \(c\) 2026 Vincent Royer'
        $license | Should -Match 'Permission is hereby granted, free of charge'
        $license | Should -Match 'THE SOFTWARE IS PROVIDED "AS IS"'
        (Get-Content (Join-Path $root 'LICENSES-ASSETS.md') -Raw) | Should -Match 'CC-BY-4.0'
        $readme = Get-Content (Join-Path $root 'README.md') -Raw
        $readme | Should -Match '\]\(LICENSE\)'
        $readme | Should -Match '\]\(LICENSES-ASSETS.md\)'
    }

    It 'every component name fits the rendered table column' {
        # A name wider than the column merges the first two columns and makes the table unparseable,
        # which is how a row name longer than the column width first showed up.
        foreach ($row in $pins.rows) {
            $row.component.Length | Should -BeLessOrEqual 22 -Because "row '$($row.component)' must fit the 24 character column"
        }
    }

    It 'stores each PLAN pin once, with detection and membership data' {
        $plan = Get-Content (Join-Path $root 'PLAN.md') -Raw
        $table = [regex]::Match($plan, '(?s)\| Component \| Pin \|\s*\| --- \| --- \|\s*(.*?)\r?\n\r?\n').Groups[1].Value
        $components = @([regex]::Matches($table, '(?m)^\| ([^|]+) \|') | ForEach-Object { $_.Groups[1].Value.Trim() })
        $components.Count | Should -BeGreaterThan 0
        foreach ($component in $components) {
            $matching = @($pins.rows | Where-Object { $_.component -eq $component })
            $matching.Count | Should -Be 1
            $matching[0].expected | Should -Not -BeNullOrEmpty
            $matching[0].detect | Should -Not -BeNullOrEmpty
            $matching[0].PSObject.Properties.Name | Should -Contain 'profiles'
        }
        $pins.device.adb_address | Should -Be '192.168.0.0:5555'
    }

    It 'parses every script and contains no PowerShell 7-only conditional syntax' {
        foreach ($file in (Get-ChildItem $scripts -Filter '*.ps1' -Recurse)) {
            $tokens = $null; $errors = $null
            [void][Management.Automation.Language.Parser]::ParseFile($file.FullName, [ref]$tokens, [ref]$errors)
            $errors.Count | Should -Be 0 -Because $file.FullName
            @($tokens | Where-Object { $_.Kind -in @('QuestionQuestion', 'QuestionQuestionEquals', 'QuestionMark') }).Count | Should -Be 0
        }
    }
}

