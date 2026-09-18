function Get-SignalCoreLayeringViolation([string]$SourceRoot) {
    # Token heuristic only. Public UnRealDashCore headers must separately hide core types.
    $root = [IO.Path]::GetFullPath($SourceRoot)
    foreach ($file in (Get-ChildItem -LiteralPath $root -Recurse -File)) {
        $relative = [IO.Path]::GetRelativePath($root, $file.FullName)
        if ($relative -match '^(SignalCore|DashboardSpec|UnRealDashCore)[\\/]') { continue }
        if ($file.Extension -notin @('.h', '.hpp', '.cpp', '.inl', '.cc')) { continue }
        Select-String -LiteralPath $file.FullName -Pattern '#\s*include\s*"SignalCore/|signal_core::|using\s+namespace\s+signal_core' |
            ForEach-Object { '{0}:{1}: {2}' -f $_.Path, $_.LineNumber, $_.Line.Trim() }
    }
}


function Get-DashboardSpecLayeringViolation([string]$SourceRoot) {
    $root = [IO.Path]::GetFullPath($SourceRoot)
    foreach ($file in (Get-ChildItem -LiteralPath $root -Recurse -File)) {
        if ($file.Extension -notin @('.h', '.hpp', '.cpp', '.inl', '.cc', '.c')) { continue }
        $relative = [IO.Path]::GetRelativePath($root, $file.FullName)
        $patterns = @()
        if ($relative -match '^DashboardSpec[\\/]') {
            if ($relative -eq 'DashboardSpec/Private/DashboardSpecModule.cpp' -or $relative -eq 'DashboardSpec\Private\DashboardSpecModule.cpp') { continue }
            $patterns = @('#\s*include\s*[<"](?:CoreMinimal|CoreTypes|Modules/|UObject/|Engine/|HAL/|Misc/|Templates/|Containers/|Components/|Blueprint/)', '\b(?:UCLASS|USTRUCT|UPROPERTY|UFUNCTION|GENERATED_BODY|IMPLEMENT_MODULE|UE_LOG|FString|FName|TArray|TMap|TSharedPtr|TUniquePtr)\b')
        } elseif ($relative -notmatch '^UnRealDashCore[\\/]Private[\\/]') {
            $patterns = @('#\s*include\s*[<"]dashboard_spec/|dashboard_spec::|using\s+namespace\s+dashboard_spec')
        }
        if ($patterns.Count) {
            Select-String -LiteralPath $file.FullName -Pattern $patterns |
                ForEach-Object { '{0}:{1}: {2}' -f $_.Path, $_.LineNumber, $_.Line.Trim() }
        }
    }
}

# PLAN 4.9: Stage 0 telemetry is receive-only, so the TCP transport must have no send path at all.
#
# This is a source check rather than only a test, because a test proves the paths it exercised and
# says nothing about the ones it did not. The suite counts bytes arriving at a real server socket
# across a full session; this says the capability is absent. Both, because either alone leaves a
# way for a send to appear later without anything noticing.
function Get-TcpTransportSendViolation {
    param([string]$SourceRoot)
    $file = Join-Path $SourceRoot 'SignalCore/Private/TcpTransport.cpp'
    if (-not (Test-Path $file)) {
        return @("TcpTransport.cpp is missing from $SourceRoot")
    }
    $violations = @()
    $lineNumber = 0
    foreach ($line in Get-Content $file) {
        $lineNumber++
        # The comment that explains the rule names the calls, so only code lines are considered.
        $code = ($line -replace '//.*$', '')
        foreach ($call in @('send', 'sendto', 'WSASend', 'write')) {
            if ($code -match ("(^|[^A-Za-z0-9_]){0}\s*\(" -f $call)) {
                $violations += "TcpTransport.cpp:${lineNumber}: receive-only transport calls $call"
            }
        }
    }
    return $violations
}
