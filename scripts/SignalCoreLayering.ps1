function Get-SignalCoreLayeringViolation([string]$SourceRoot) {
    # Token heuristic only. Public UnRealDashCore headers must separately hide core types.
    $root = [IO.Path]::GetFullPath($SourceRoot)
    foreach ($file in (Get-ChildItem -LiteralPath $root -Recurse -File)) {
        $relative = [IO.Path]::GetRelativePath($root, $file.FullName)
        if ($relative -match '^(SignalCore|UnRealDashCore)[\\/]') { continue }
        if ($file.Extension -notin @('.h', '.hpp', '.cpp', '.inl', '.cc')) { continue }
        Select-String -LiteralPath $file.FullName -Pattern '#\s*include\s*"SignalCore/|signal_core::|using\s+namespace\s+signal_core' |
            ForEach-Object { '{0}:{1}: {2}' -f $_.Path, $_.LineNumber, $_.Line.Trim() }
    }
}
