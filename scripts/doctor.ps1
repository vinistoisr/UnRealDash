<#
.SYNOPSIS
Checks only the selected profile's prerequisites from pins.json.
.DESCRIPTION
Requires PowerShell 7; syntax remains parseable by Windows PowerShell 5.1.
-SearchPath replaces PATH for command-line tool discovery, including child tools.
-PinsFile replaces scripts/pins.json for controlled tests and local pin data.
PreInstall selects only capacity and installer prerequisites, not installed pins.
#>
[CmdletBinding()]
param(
    [ValidateSet('workstation', 'android', 'linux', 'device')][string]$Profile = 'workstation',
    [switch]$PreInstall,
    [AllowEmptyString()][string]$SearchPath = $env:PATH,
    [string]$PinsFile = (Join-Path $PSScriptRoot 'pins.json')
)
# This guard is duplicated on purpose so each entry point fails clearly under Windows PowerShell 5.1.
if ($PSVersionTable.PSVersion.Major -lt 7) { Write-Output 'PowerShell 7 or later is required. Run this script with pwsh.'; exit 1 }
$ErrorActionPreference = 'Stop'
$env:PATH = $SearchPath

function Find-Tool([string]$Name, [string]$Directories = $SearchPath) {
    foreach ($directory in ($Directories -split [IO.Path]::PathSeparator)) {
        if ([string]::IsNullOrWhiteSpace($directory)) { continue }
        foreach ($extension in @('.exe', '.cmd', '.bat', '.ps1', '')) {
            $candidate = Join-Path $directory.Trim('"') ($Name + $extension)
            if (Test-Path -LiteralPath $candidate -PathType Leaf) {
                $item = Get-Item -LiteralPath $candidate
                if ($item.LinkType -eq 'SymbolicLink') { return $item.ResolveLinkTarget($true).FullName }
                return $item.FullName
            }
        }
    }
    throw "$Name not found on search PATH"
}

function Invoke-Tool([string]$Path, [string[]]$Arguments) {
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = $Path
    $start.UseShellExecute = $false
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $start.CreateNoWindow = $true
    switch ([IO.Path]::GetExtension($Path).ToLowerInvariant()) {
        { $_ -in @('.cmd', '.bat') } {
            $start.FileName = Join-Path $env:SystemRoot 'System32/cmd.exe'
            $quoted = @(@($Path) + @($Arguments) | ForEach-Object { '"' + $_.Replace('"', '""') + '"' })
            $start.Arguments = '/d /c "' + ($quoted -join ' ') + '"'
        }
        '.ps1' {
            $start.FileName = Join-Path $PSHOME 'pwsh.exe'
            foreach ($argument in (@('-NoProfile', '-File', $Path) + $Arguments)) { $start.ArgumentList.Add($argument) }
        }
        default { foreach ($argument in $Arguments) { $start.ArgumentList.Add($argument) } }
    }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $start
    try {
        [void]$process.Start()
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit(15000)) { $process.Kill($true); throw 'tool timed out after 15 seconds' }
        # The process has exited, but a daemon it spawned can still hold the inherited pipe, so the
        # reads are bounded too. adb is the known case; the bound keeps any such tool from hanging a run.
        if (-not [Threading.Tasks.Task]::WaitAll(@($stdout, $stderr), 5000)) {
            throw 'tool exited but left its output pipe open for more than 5 seconds'
        }
        $output = ($stdout.GetAwaiter().GetResult() + $stderr.GetAwaiter().GetResult()).Trim()
        if ($process.ExitCode -ne 0) { throw "tool exit $($process.ExitCode): $output" }
        return $output
    } finally { $process.Dispose() }
}

function Get-VisualStudio([string[]]$Requirements) {
    $path = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $arguments = @('-all', '-products', '*', '-format', 'json')
    if ($Requirements.Count -gt 0) { $arguments += '-requires'; $arguments += $Requirements }
    return @( (Invoke-Tool $path $arguments) | ConvertFrom-Json )
}

# The installed engine states which Android SDK platform, build-tools, cmake and NDK it needs.
# Reading it here means the doctor cannot drift from the engine the way a duplicated pin can.
# adb ships inside the Android SDK's platform-tools, which is not on PATH by default.
# Look there first so a correct SDK install does not need a PATH edit, then fall back to the search PATH.
function Find-Adb {
    # The search path wins, so -SearchPath stays a usable seam and an explicit adb is respected.
    # Otherwise fall back to the SDK's platform-tools, which is where a correct install puts it.
    $adb = $null
    try { $adb = Find-Tool 'adb' } catch { $adb = $null }
    if (-not $adb) {
        $sdk = Get-AndroidSdkRoot
        if ($sdk) {
            $candidate = Join-Path $sdk 'platform-tools/adb.exe'
            if (Test-Path -LiteralPath $candidate -PathType Leaf) { $adb = $candidate }
        }
    }
    if (-not $adb) { throw 'adb not found on the search PATH or in the Android SDK' }
    # The first adb command starts a daemon that inherits the caller's pipes. Start it here with no
    # redirection so the queries that follow return their own output and nothing keeps a pipe open.
    if (-not $script:AdbServerStarted -and $adb -like '*.exe') {
        $script:AdbServerStarted = $true
        $start = [Diagnostics.ProcessStartInfo]::new()
        $start.FileName = $adb
        $start.UseShellExecute = $false
        $start.CreateNoWindow = $true
        [void]$start.ArgumentList.Add('start-server')
        $server = [Diagnostics.Process]::Start($start)
        [void]$server.WaitForExit(15000)
        $server.Dispose()
    }
    return $adb
}

function Get-EngineAndroidManifest($Pins) {
    $path = Join-Path $Pins.engine.root 'Engine/Config/Android/Android_SDK.json'
    if (-not (Test-Path -LiteralPath $path)) { throw "engine Android manifest not found at $path" }
    return Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
}

function Get-AndroidSdkRoot {
    if ($env:ANDROID_HOME) { return $env:ANDROID_HOME }
    if ($env:ANDROID_SDK_ROOT) { return $env:ANDROID_SDK_ROOT }
    return (Join-Path $env:LOCALAPPDATA 'Android/Sdk')
}

function Test-Row($Row, $Pins) {
    $found = 'missing'
    $ok = $false
    try {
        switch ($Row.detect) {
            'disk' {
                $free = (Get-PSDrive C).Free / 1GB
                $found = '{0:F2} GB' -f $free
                $ok = $free -ge $Row.minimum
            }
            'command' {
                $found = Invoke-Tool (Find-Tool $Row.command) $Row.arguments
                $ok = $found -match $Row.pattern
            }
            'jdk' {
                $source = 'search PATH'
                $java = $null
                if ($env:JAVA_HOME) {
                    try {
                        $java = Find-Tool $Row.command (Join-Path $env:JAVA_HOME 'bin')
                        $source = "JAVA_HOME ($env:JAVA_HOME)"
                    } catch { $java = $null }
                }
                if (-not $java) { $java = Find-Tool $Row.command }
                $output = Invoke-Tool $java $Row.arguments
                $major = [regex]::Match($output, '(?m)version "(\d+)').Groups[1].Value
                $found = "$source; $(($output -split '\r?\n')[0])"
                $ok = $major -and ([int]$major -ge [int]$Row.minimum_major)
            }
            'presence' { $found = Find-Tool $Row.command; $ok = $true }
            'powershell' { $found = $PSVersionTable.PSVersion.ToString(); $ok = $PSVersionTable.PSVersion.Major -ge $Row.minimum }
            'identity' { $found = [Security.Principal.WindowsIdentity]::GetCurrent().Name; $ok = -not [string]::IsNullOrWhiteSpace($found) }
            'path-writable' {
                # Opening with write access tests the permission without changing the registry.
                $key = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey('Environment', $true)
                if ($null -eq $key) { throw 'user Environment key unavailable' }
                $key.Dispose()
                $found = 'user Environment key writable'; $ok = $true
            }
            'vs' {
                $installations = @(Get-VisualStudio $Row.workloads | Where-Object { $_.productId -eq $Row.product -and $_.isComplete })
                $found = ($installations.installationVersion -join ', ')
                if (-not $found) { $found = 'Community IDE with both C++ workloads not found' }
                $ok = @($installations | Where-Object {
                    ([version]$_.installationVersion).Major -eq $Row.major -and
                    [version]$_.installationVersion -ge [version]$Row.minimum
                }).Count -gt 0
            }
            'msvc' {
                $versions = @(foreach ($installation in (Get-VisualStudio @())) {
                    Get-ChildItem (Join-Path $installation.installationPath 'VC/Tools/MSVC') -Directory -ErrorAction SilentlyContinue |
                        Where-Object { Test-Path (Join-Path $_.FullName 'bin/Hostx64/x64/cl.exe') } | ForEach-Object { $_.Name }
                })
                $found = $versions -join ', '; $ok = @($versions | Where-Object { $_ -like "$($Row.version).*" }).Count -gt 0
            }
            'windows-sdk' {
                $sdk = $Row.kits_root
                if (-not $sdk) {
                    try { $sdk = (Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows Kits\Installed Roots' -Name KitsRoot10 -ErrorAction Stop).KitsRoot10 }
                    catch { $sdk = $null }
                    if (-not $sdk) { $sdk = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits/10' }
                }
                $versions = @(Get-ChildItem (Join-Path $sdk 'Include') -Directory -ErrorAction SilentlyContinue | ForEach-Object { $_.Name })
                $found = "$sdk; Include versions: $($versions -join ', ')"
                $ok = @($versions | Where-Object { $_ -eq $Row.version -or $_ -like "$($Row.version).*" }).Count -gt 0
            }
            'engine' {
                $engine = Join-Path $Pins.engine.root 'Engine'
                $version = Get-Content (Join-Path $engine 'Build/Build.version') -Raw | ConvertFrom-Json
                $found = "$($version.MajorVersion).$($version.MinorVersion).$($version.PatchVersion)"
                $ok = $found -eq $Row.version -and (Test-Path (Join-Path $engine 'Build/InstalledBuild.txt'))
            }
            'engine-platform' {
                # Ask UnrealBuildTool whether it can build this target. A launcher install ships the
                # editor's target-platform modules for platforms whose build support was never
                # downloaded, so the file layout cannot answer this and the build fails much later.
                $batch = Join-Path $Pins.engine.root 'Engine/Build/BatchFiles/Build.bat'
                if (-not (Test-Path -LiteralPath $batch)) { throw "engine Build.bat not found at $batch" }
                $output = Invoke-Tool $batch @('-Mode=ValidatePlatforms', "-Platforms=$($Row.platform)")
                $match = [regex]::Match($output, "##PlatformValidate:\s+$($Row.platform)\s+(\w+)")
                $state = $match.Groups[1].Value
                $found = if ($state) { "UnrealBuildTool reports $($Row.platform) $state" } else { $output }
                $ok = $state -eq 'VALID'
                if (-not $ok -and $state -eq 'INVALID') {
                    $found += "; add $($Row.platform) as a target platform in the Epic Games Launcher"
                }
            }
            'android-sdk' {
                $sdk = Get-AndroidSdkRoot
                $manifest = Get-EngineAndroidManifest $Pins
                $platform = Test-Path -LiteralPath (Join-Path $sdk "platforms/$($manifest.platforms)/android.jar") -PathType Leaf
                $buildTools = Test-Path -LiteralPath (Join-Path $sdk "build-tools/$($manifest.'build-tools')") -PathType Container
                $cmake = Test-Path -LiteralPath (Join-Path $sdk "cmake/$($manifest.cmake)") -PathType Container
                $found = "${sdk}; $($manifest.platforms): $platform; build-tools $($manifest.'build-tools'): $buildTools; cmake $($manifest.cmake): $cmake"
                $ok = $platform -and $buildTools -and $cmake
            }
            'ndk' {
                $sdk = Get-AndroidSdkRoot
                $manifest = Get-EngineAndroidManifest $Pins
                $properties = Join-Path $sdk "ndk/$($manifest.ndk)/source.properties"
                $found = "$($manifest.MainVersion) ($($manifest.ndk))"
                $ok = Test-Path -LiteralPath $properties -PathType Leaf
                if (-not $ok) { $found += "; not installed under $sdk" }
            }
            'studio' {
                $studio = $env:ANDROID_STUDIO_HOME
                if (-not $studio) { $studio = Join-Path $env:ProgramFiles 'Android/Android Studio' }
                $product = Join-Path $studio 'product-info.json'
                $runtime = Join-Path $studio 'jbr/bin/java.exe'
                $version = 'unknown'
                if (Test-Path -LiteralPath $product) {
                    $info = Get-Content -LiteralPath $product -Raw | ConvertFrom-Json
                    $version = $info.version
                    if ($info.versionSuffix) { $version += " $($info.versionSuffix)" }
                }
                $found = "$studio; version $version; bundled runtime: $(Test-Path -LiteralPath $runtime)"
                $ok = (Test-Path -LiteralPath $product) -and (Test-Path -LiteralPath $runtime)
            }
            'environment' { $found = [Environment]::GetEnvironmentVariable($Row.variable); $ok = $found -and (Test-Path -LiteralPath $found -PathType Container) }
            'linux-toolchain' {
                if (-not $env:LINUX_MULTIARCH_ROOT) { throw 'LINUX_MULTIARCH_ROOT is unset' }
                $clang = Find-Tool 'clang++' (Join-Path $env:LINUX_MULTIARCH_ROOT 'x86_64-unknown-linux-gnu/bin')
                $found = Invoke-Tool $clang @('--version')
                $version = [regex]::Match($found, '\bclang version\s+(\d+\.\d+\.\d+)(?![\d.])').Groups[1].Value
                $ok = $version -eq $Row.version
            }
            'adb' {
                $found = Invoke-Tool (Find-Adb) @('-s', $Pins.device.adb_address, 'get-state')
                $ok = $found.Trim() -eq 'device'
            }
            'device-storage' {
                $output = Invoke-Tool (Find-Adb) @('-s', $Pins.device.adb_address, 'shell', 'df', '-k', '/data')
                $line = @($output -split '\r?\n' | Where-Object { $_.Trim() -match '^/.*\s/data\s*$' }) | Select-Object -Last 1
                $fields = $line.Trim() -split '\s+'
                $available = 0L
                if ($fields.Count -lt 6 -or -not [long]::TryParse($fields[3], [ref]$available)) { throw "unrecognized df output: $output" }
                $found = "$available KiB available on /data"; $ok = $available -gt $Row.minimum
            }
            default { throw "unknown detection method: $($Row.detect)" }
        }
    } catch { $found = $_.Exception.Message; $ok = $false }
    if ([string]::IsNullOrWhiteSpace($found)) { $found = 'not found' }
    $status = 'FAIL'
    if ($ok) { $status = 'PASS' }
    [pscustomobject]@{ Component = $Row.component; Expected = $Row.expected; Found = ($found -replace '\s+', ' '); Status = $status }
}

try {
    $pins = Get-Content -LiteralPath $PinsFile -Raw | ConvertFrom-Json
    $mode = $Profile
    if ($PreInstall) { $mode = 'preinstall' }
    $selected = @($pins.rows | Where-Object { $_.profiles -contains $mode })
    if ($selected.Count -eq 0) { throw "No rows configured for $mode" }
    $results = @($selected | ForEach-Object { Test-Row $_ $pins })
    Write-Output "Doctor profile: $mode"
    $columns = @(
        # Wide enough for the longest component name plus a separating space. A name that overflows
        # this width silently merges the first two columns in the rendered table, so a test asserts
        # every configured name fits.
        @{ Expression = 'Component'; Width = 24 }
        @{ Expression = 'Expected'; Width = 57 }
        @{ Expression = 'Found'; Width = 78 }
        @{ Expression = 'Status'; Width = 6 }
    )
    $results | Format-Table -Property $columns -Wrap | Out-String -Width 180 | Write-Output
    if (@($results | Where-Object { $_.Status -ne 'PASS' }).Count) { exit 1 }
    exit 0
} catch { Write-Output "Doctor failed: $($_.Exception.Message)"; exit 1 }
