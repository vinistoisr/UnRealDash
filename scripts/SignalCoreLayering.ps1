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
