# Device half of the PLAN 4.4 gate, and a staging defect it found

Date: 2026-09-17. Device: Pixel 10 Pro, Vulkan, Development. PLAN 4.4 criterion 8.

## How the device half runs

The desktop half of this gate is a commandlet, which needs the editor and therefore cannot run on
Android. On device the packaged player is launched once per fixture with `-udash=<path>` and the
verdict is read back from logcat.

`ADashPackageHUD` emits exactly one machine-readable line per run:

```
DashVerdict accepted=<true|false> code=<CodeName> pointer=<json pointer> path=<path>
```

That line was added for this gate. Without it an accepted package logs nothing at all, so success
would have been indistinguishable from the app dying during startup, which is the kind of
absence-as-evidence this project has already been bitten by.

The runner is `scripts/run-device-package-gate.ps1`. Expected outcomes come from
`tests/fixtures/packages/cases.json` at runtime, never from a list in the script. The command
line goes through `UECommandLine.txt`, which **replaces** the whole command line and therefore
carries `-project=` itself, and the map is launched with
`?game=/Script/UnRealDash.DashPackageGameMode` because the cooked map defaults to the smoke
spike's game mode.

## The defect: staged non-UFS files never reach the device

The first real run rejected `well-formed.udash`, which the desktop gate accepts. The startup log
the spec asked for pinned it immediately:

```
Dashboard schema directory: /storage/emulated/0/Android/data/com.unrealdash.player/files/
                            UnrealGame/UnRealDash/UnRealDash/Schema
Code: E_SCHEMA
Message: cannot read schema dashboard
DashVerdict accepted=false code=E_SCHEMA pointer= path=.../well-formed.udash
```

That directory does not exist on the device.

The `RuntimeDependency` in `DashboardSpec.Build.cs` is **not** at fault. It stages correctly:

```
runtime/UnRealDash/Saved/StagedBuilds/Android/UnRealDash/Schema/dashboard.schema.json
                                                             /definition-pack.schema.json
                                                             /manifest.schema.json
                                                             /showcase.schema.json
                                                             /signals.schema.json
```

The failure is one step later, in delivery. Three things are verified:

1. The five files are listed in the staged non-UFS manifest,
   `Saved/StagedBuilds/Android/Manifest_NonUFSFiles_Android.txt`, so staging did its job.
2. Listing the built APK finds **zero** schema entries. With `bPackageDataInsideApk=True` the
   cooked UFS data ships as `assets/main.obb.png`, and non-UFS files are not carried in it.
3. The generated `Install_UnRealDash-arm64.apk` installer script uninstalls, installs the APK and
   clears directories. The blocks where non-UFS pushes would appear are **empty**.

So in this configuration nothing delivers the files: they are neither inside the APK nor pushed by
the installer.

**Corrected from an earlier version of this report.** It first said the `StagedFileType.NonUFS`
mechanism is simply wrong for Android. That was concluded from "zero entries in the APK" alone and
went further than the evidence. `AndroidPlatform.Automation.cs` shows UAT's own deploy step pushing
non-UFS files by `adb push` from a manifest delta, so a full `RunUAT -deploy` may well deliver
them. Whether it does is **not tested**, because the device is not available, and the gate here was
run against a manual `adb install -r` of the APK, which skips the deploy step entirely.

What that means for the fix: the choice is not "the mechanism is broken" but "the project should
not depend on a deploy step it does not use". The gate, and any driveway or head-unit workflow,
installs an APK directly. A delivery route that only works through `RunUAT -deploy` would keep
failing in exactly the situation the desk device exists for.

## What this does and does not prove

Pushing the five schema files to the resolved path by hand makes the same fixture pass:

```
DashVerdict accepted=true code= pointer= path=.../well-formed.udash
```

So **the loader, the validator and the widget builder work on the device**. What is broken is
only the delivery of the schema files into the package. The gate results recorded alongside this
report were produced with the schemas pushed manually, and that caveat travels with them: they
are evidence about the loader, not about packaging.

## Answered: what deploy actually does

`RunUAT ... -deploy` was run against the device. It **does** push the non-UFS files, all five
schemas included, and it pushes them to:

```
/sdcard/UnrealGame/UnRealDash/UnRealDash/Schema/
```

The player reads from the app-specific external directory instead:

```
/storage/emulated/0/Android/data/com.unrealdash.player/files/UnrealGame/UnRealDash/...
```

because `bUseExternalFilesDir=True`, which the 4.0 smoke spike set deliberately: that tree needs no
runtime permission and `adb push` and `adb pull` reach it without root.

So nothing is broken in staging or in deploy. The two simply target different directories, and this
project's configuration puts the app on the far side of that gap. Neither of the earlier diagnoses
in this report was right: the mechanism is not wrong for Android, and the files are not undelivered
because of the APK. They are delivered to a path the app does not read.

`scripts/deploy-device-content.ps1` closes it, driven by `Manifest_NonUFSFiles_Android.txt` so it
stays correct as the non-UFS set changes rather than hardcoding "the schemas". With it the app
accepts `well-formed.udash`.

None of the three invasive options is needed. Compiling the schemas in, which would have changed
`Validator`'s constructor, is off the table.

**Do not run `RunUAT -deploy` against this project.** It reinstalls the APK, which wipes the
app-specific tree including pushed fixtures, and it leaves a staged copy at `/sdcard/UnrealGame`
that changes how the engine resolves the relative `-project=` path, after which every launch fails
with "Failed to open descriptor file". Both were hit during this session. Install the APK and run
`deploy-device-content.ps1`.

## Status

PLAN 4.4 criterion 8 is **not met**, for a smaller reason than before.

The delivery question is answered and solved by `deploy-device-content.ps1`. What remains is
mechanical: the APK on the device was built at 16:47 and batch mode was added to the source at
16:56, so the device build does not understand `-udash-batch`. The batch gate cannot run until
Android is repackaged. That is one package and two launches, and it was a sequencing mistake on my
part, not a defect in anything.
