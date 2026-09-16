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
                if ($env:JAVA_HOME) { $source = "JAVA_HOME ($env:JAVA_HOME)" }
                try {
                    if ($env:JAVA_HOME) { $java = Find-Tool $Row.command (Join-Path $env:JAVA_HOME 'bin') }
                    else { $java = Find-Tool $Row.command }
                    $version = Invoke-Tool $java $Row.arguments
                    $found = "${source}: $version"
                    $ok = $version -match $Row.pattern
                } catch { $found = "${source}: $($_.Exception.Message)"; $ok = $false }
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
                foreach ($platform in $Row.platforms) {
                    if (-not (Test-Path (Join-Path $engine "Platforms/$platform"))) { $ok = $false; $found += "; $platform missing" }
                }
            }
            'android-sdk' {
                $sdk = Get-AndroidSdkRoot
                $platform = Test-Path -LiteralPath (Join-Path $sdk "platforms/android-$($Row.target)/android.jar") -PathType Leaf
                $buildTools = @(Get-ChildItem (Join-Path $sdk 'build-tools') -Directory -ErrorAction SilentlyContinue)
                $found = "${sdk}; android-$($Row.target): $platform; build-tools: $($buildTools.Name -join ', ')"
                $ok = $platform -and $buildTools.Count -gt 0
            }
            'ndk' {
                $ndk = $env:NDKROOT
                if (-not $ndk) {
                    $ndk = Join-Path (Get-AndroidSdkRoot) "ndk/$($Row.version)"
                }
                $properties = Get-Content (Join-Path $ndk 'source.properties') -Raw
                $found = ([regex]::Match($properties, 'Pkg.Revision\s*=\s*([^\r\n]+)')).Groups[1].Value.Trim()
                $ok = $found -eq $Row.version
            }
            'studio' {
                $studio = $env:ANDROID_STUDIO_HOME
                if (-not $studio) { $studio = Join-Path $env:ProgramFiles 'Android/Android Studio' }
                $product = Get-Content (Join-Path $studio 'product-info.json') -Raw | ConvertFrom-Json
                $found = $product.version
                if ($product.versionSuffix) { $found += " $($product.versionSuffix)" }
                $ok = $found -eq "$($Row.version) Patch $($Row.patch)"
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
                $found = Invoke-Tool (Find-Tool 'adb') @('-s', $Pins.device.adb_address, 'get-state')
                $ok = $found.Trim() -eq 'device'
            }
            'device-storage' {
                $output = Invoke-Tool (Find-Tool 'adb') @('-s', $Pins.device.adb_address, 'shell', 'df', '-k', '/data')
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
        @{ Expression = 'Component'; Width = 22 }
        @{ Expression = 'Expected'; Width = 57 }
        @{ Expression = 'Found'; Width = 78 }
        @{ Expression = 'Status'; Width = 6 }
    )
    $results | Format-Table -Property $columns -Wrap | Out-String -Width 180 | Write-Output
    if (@($results | Where-Object { $_.Status -ne 'PASS' }).Count) { exit 1 }
    exit 0
} catch { Write-Output "Doctor failed: $($_.Exception.Message)"; exit 1 }
