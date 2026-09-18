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

## Options for the fix, none of them chosen yet

0. First, cheaply, confirm whether `RunUAT ... -deploy` pushes them, since the manifest and the
   automation source both suggest it should. That is one device run and it decides whether any of
   the options below are needed at all. Even if it works, options 1 to 3 remain worth considering,
   because a plain APK install must also work.
1. Add the schemas to the APK through the UPL or `ExtraFilesToPackage` so they land in `assets/`
   and are extracted into the `UnrealGame` tree at first run, next to where the resolver already
   looks.
2. Cook them as UFS and have the player extract them to a real path at startup before the
   validator runs. This keeps one delivery mechanism but adds a startup copy.
3. Compile them in. All five total about 51 KB. This removes the file dependency on every
   platform at once and deletes a whole class of path-resolution bug, but it changes
   `Validator`'s constructor, which currently takes a directory, so it is an API change and not a
   packaging tweak. It also moves the schemas away from being data, which docs/ARCHITECTURE.md
   prefers, so it needs a deliberate decision rather than a quiet one.

Option 3 is the most robust and the most invasive. The choice belongs with the owner.

## Status

PLAN 4.4 criterion 8 is **not met**. The loader half is proven on device; the packaging half is
not, and the chunk stays "built, not complete" until the schemas reach the device without help.
