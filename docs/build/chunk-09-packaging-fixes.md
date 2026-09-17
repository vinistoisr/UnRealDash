# Build chunk 09: packaging script defects

Status: frozen spec for a Codex build session. Covers defects in the PLAN 4.0 packaging scripts found by an independent review on 2026-09-17. Read docs/ARCHITECTURE.md ("Testing shape", "What is not allowed") before writing anything.

## Goal

`scripts/lib/package.ps1` and its two wrappers are the path every later device task runs through. Four defects were found and **all four were reproduced on this machine**; the reproductions are quoted below so you do not have to rediscover them. Fix them and close the test gap that let them through.

Do not refactor beyond these four items. Do not rename anything. Do not change the UAT argument list, the archive layout or the RHI ini contents.

## The four defects, with their reproductions

### D1 (BLOCKER). The scripts cannot run in Windows PowerShell 5.1.

`scripts/lib/package.ps1:7` resolves the doctor host as `Join-Path $PSHOME 'pwsh.exe'`. Under Windows PowerShell 5.1, `$PSHOME` is the 5.1 install directory, which never contains `pwsh.exe`. The owner's primary interactive shell is Windows PowerShell 5.1.

Reproduced:

```
> powershell.exe -NoProfile -Command "Write-Output $PSHOME"
C:\Windows\System32\WindowsPowerShell\v1.0

> powershell.exe -NoProfile -File scripts/package-windows.ps1 -WhatIf -DoctorScript scripts/tests/fakes/doctor-pass.ps1
Doctor profile: workstation
package-windows.ps1 : The term 'C:\Windows\System32\WindowsPowerShell\v1.0\pwsh.exe' is not recognized ...
exit code = 1
```

It fails loudly rather than silently, which is the one good thing about it, but packaging is unavailable from the shell the owner actually uses.

`scripts/tests/SmokePackage.Tests.ps1:7` repeats the same assumption and has the same defect.

### D2 (MAJOR). A cleanup failure escapes the handler and destroys the exit code.

`scripts/lib/package.ps1:47`:

```powershell
} finally {
    if ($ownsGenerated) { Remove-Item -LiteralPath $generated -Force }
}
```

With `$ErrorActionPreference = 'Stop'`, a failing `Remove-Item` throws **from `finally`**, where the function's own `catch` cannot see it. The `return` value is discarded and the wrapper's `exit (Invoke-SmokePackage ...)` never receives it. A packaging run that failed with UAT code 7, or one that succeeded, both end up reporting whatever the escaping exception produces, and the ini is still on disk.

Reproduced:

```powershell
function Test-Fn {
    $ErrorActionPreference = 'Stop'
    try { return 7 } finally { Remove-Item -LiteralPath 'C:\does\not\exist\nope.ini' -Force }
}
try { $r = Test-Fn; "returned: $r" } catch { "ESCAPED from finally: $($_.Exception.GetType().Name)" }
# -> ESCAPED from finally: ItemNotFoundException
```

Real causes: another process holding the ini open without delete sharing, a denied delete, or an antivirus scanner. `-Force` does not overcome any of those.

### D3 (MAJOR). No test exercises the real packaging path, so the ini lifecycle is untested.

Every case in `scripts/tests/SmokePackage.Tests.ps1` passes `-WhatIf`. `scripts/lib/package.ps1:26` returns before the ini is ever created and before UAT is ever invoked. **Deleting the entire `finally` block leaves all nine tests passing.**

Consequently these two assertions prove less than they look like they prove:

- `scripts/tests/SmokePackage.Tests.ps1:25` and `:37`, `Test-Path -LiteralPath $generated | Should -BeFalse`. On the preview path no file was ever created, so these cannot detect a cleanup failure. They retain a little value: they would catch a preview that started writing the ini, or a stale file left by an earlier run. They do not test cleanup.
- `:43` and `:55`, `Should -Not -Match 'RunUAT'`. These check printed output, not that a UAT process was never started.

Untested entirely: the ini's bytes as UAT actually sees them, cleanup after success, cleanup after a nonzero UAT exit, cleanup after a throw, preservation of a pre-existing ini, and propagation of an arbitrary UAT exit code.

Note for your report: an earlier review of this project rejected assertions that could not fail. Do not add any here.

### D4 (MAJOR, latent on this machine). A repository path containing `&` breaks packaging and makes cmd.exe run part of the path.

`RunUAT.bat` is a batch file. PowerShell quotes a native argument when it contains whitespace, but not when it contains only cmd metacharacters, so the argument crosses into `cmd.exe` unprotected.

Reproduced against a probe batch file:

```
> & echoargs.bat '-project=C:\A&B\UnRealDash\x.uproject' '-archivedirectory=C:\Has Space\out' '-platform=Win64'
ARG=[-project]
ARG=[C:\A]
The system cannot find the path specified.
exit=1
```

`cmd.exe` split on the `&` and attempted to execute `B\UnRealDash\x.uproject` as a command. The path with a space was handled correctly, so this is specifically a metacharacter defect, not a spaces defect. The current checkout is `C:\Users\Vincent\UnRealDash`, so it does not bite today; it bites the first person who clones into a path containing `&`, `^`, `|`, `<`, `>` or `%`.

`%` additionally risks environment-variable expansion inside the batch file.

## Deliverables

### 1. Resolve the PowerShell 7 host properly (D1)

Add one function in `scripts/lib/package.ps1`, `Resolve-PwshPath`, that returns the path to `pwsh.exe` by trying, in order:

1. `(Get-Process -Id $PID).Path` when the current host is already PowerShell 7 (`$PSVersionTable.PSEdition -eq 'Core'`).
2. `(Get-Command pwsh -CommandType Application -ErrorAction SilentlyContinue).Source`.
3. The default install locations `$env:ProgramFiles\PowerShell\7\pwsh.exe` and `$env:LOCALAPPDATA\Microsoft\PowerShell\7\pwsh.exe`.

If none resolves, print a message naming what it looked for and return a nonzero result the same way a doctor failure does. Do not silently fall back to Windows PowerShell: doctor.ps1 is a PowerShell 7 script and running it under 5.1 would produce a misleading result, which docs/ARCHITECTURE.md forbids ("Silent fallbacks").

Apply the same resolution in `scripts/tests/SmokePackage.Tests.ps1`'s `Invoke-PackagePreview` helper. Dot-source `scripts/lib/package.ps1` from the test rather than duplicating the logic.

### 2. Make cleanup unable to destroy the result (D2)

Move the `Remove-Item` inside its own `try`/`catch` within the `finally` block. On failure: print a message naming the file and the reason, and make the function return a nonzero code. If the run had already failed, keep the original nonzero code. If the run had succeeded, return a distinct nonzero code, because a package whose override ini is still on disk is not a clean success and the next Android run will refuse to start.

Record the chosen code in the script's comments and in your report.

### 3. Guard paths that cmd.exe would reinterpret (D4)

Before building the argument list, check the resolved `$uat`, `$project` and `$archive` for any of `& ^ | < > %`. If any is present, print a message naming the offending path and the character, and return nonzero without invoking UAT.

Do not attempt general batch-safe quoting. Escaping arbitrary cmd metacharacters through a batch file's argument parsing is not reliably solvable, and docs/ARCHITECTURE.md prefers a degraded path that says it is degraded over a silent one. A clear refusal is the correct behaviour here. Say so in a comment.

### 4. Make the real path testable, and test it (D3)

Add an optional parameter to `Invoke-SmokePackage`, `-UatPath`, defaulting to the value resolved from `scripts/pins.json` exactly as today. This is the only structural change permitted, and it exists so a test can substitute a fake UAT. Do not add a parameter for anything else.

Add a fake UAT under `scripts/tests/fakes/`. It must be a real `.bat` file, because the D4 defect and the argument-passing behaviour only exist at the batch boundary and a `.ps1` fake would not reproduce them. It records the arguments it received and a copy of `GeneratedEngine.ini` as it existed at invocation time into a directory given by an environment variable, then exits with a code given by another environment variable.

New Pester cases, none of them using `-WhatIf`:

1. Android success: the ini exists at UAT invocation with exactly the expected bytes for the requested RHI, and is gone after the function returns 0.
2. Android UAT failure: fake UAT exits 7, the function returns 7, and the ini is gone.
3. Windows: no ini is created at any point.
4. Pre-existing ini: the function returns nonzero, does not invoke UAT, and **leaves the existing file in place**.
5. Cleanup failure: hold the ini open with a deny-delete share from the test, and assert the function returns nonzero and prints the file name. Release the handle in a `finally` so the test cannot leak it.
6. Exit-code propagation: fake UAT exits 3, then 0; assert the function returns each.
7. D4 guard: an archive path containing `&` returns nonzero, names the character, and does not invoke UAT (the fake records nothing).

Every one of these must fail if you revert the corresponding fix. State in your report that you checked that, one line per case.

Keep the nine existing preview cases passing unchanged.

## Constraints

- No em dashes in any file, comment or printed string.
- No new dependency. Pester 5/6 and PowerShell 7 only.
- Tests must not leave files behind in `runtime/UnRealDash/Config/`. Use the real path the script uses, but guarantee cleanup in a `finally`, and fail the test rather than skipping if a stale file is present at the start.
- `scripts/package-linux.ps1` still uses `Invoke-FoundationScript` from `scripts/lib/common.ps1` and is out of scope. Do not touch it.

## Non-goals

- No change to the UAT argument list, archive directory layout, or ini contents.
- No real packaging run. You have no engine and no Android toolchain.
- No change to `doctor.ps1` beyond nothing at all.

## Pass/fail criteria

1. `pwsh -NoProfile -Command "Invoke-Pester -Path scripts/tests -CI -Output Detailed"` passes with the nine original cases plus the seven new ones, zero failures.
2. `powershell.exe -NoProfile -File scripts/package-windows.ps1 -WhatIf -DoctorScript scripts/tests/fakes/doctor-pass.ps1` runs the doctor and prints the preview under **Windows PowerShell 5.1**, exit 0.
3. Same for `scripts/package-android.ps1 -Rhi vulkan`, exit 0, and without `-Rhi`, exit 1.
4. `pwsh -NoProfile -File scripts/doctor.ps1 -Profile workstation` still exits 0.
5. Reverting any one of the four fixes makes at least one named test fail.

## Proof

Run these and paste the output verbatim:

```
pwsh -NoProfile -Command "Invoke-Pester -Path scripts/tests -CI -Output Detailed"
powershell.exe -NoProfile -File scripts/package-windows.ps1 -WhatIf -DoctorScript scripts/tests/fakes/doctor-pass.ps1; echo "exit=$LASTEXITCODE"
powershell.exe -NoProfile -File scripts/package-android.ps1 -Rhi vulkan -WhatIf -DoctorScript scripts/tests/fakes/doctor-pass.ps1; echo "exit=$LASTEXITCODE"
powershell.exe -NoProfile -File scripts/package-android.ps1 -WhatIf -DoctorScript scripts/tests/fakes/doctor-pass.ps1; echo "exit=$LASTEXITCODE"
pwsh -NoProfile -File scripts/doctor.ps1 -Profile workstation; echo "exit=$LASTEXITCODE"
git status --short
```

Your proof is advisory. Claude re-runs all of it.

## Report format

End with: files added or changed (one line each: path, what, which defect), the proof output verbatim, the cleanup-failure exit code you chose and why, one line per new test saying which fix it fails without, any deviation from this spec with the reason, and anything you could not do.
