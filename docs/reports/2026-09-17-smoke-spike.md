# PLAN 4.0, the smoke spike: what it proved and what it found

Run 2026-09-17 on the workstation and on the desk device, a Pixel 10 Pro over USB. Device facts are in `device-pixel10pro.md`. The head unit is not covered here; that is 4.0b.

## Result

The Windows half of the gate is met. The Android half is met in Development under both RHIs. The Shipping configuration path is **not** met and is recorded below as a finding rather than worked around.

| Gate clause | State | Evidence |
| --- | --- | --- |
| Windows package renders and writes a metrics CSV | met | 1,495 frames, `Saved/SmokeOut/Windows/` |
| Vulkan APK installs, launches, renders | met | `logs/pixel-vulkan/screenshot.png` |
| GLES APK installs, launches, renders | met | `logs/pixel-gles/screenshot.png`, log reports `Smoke RHI=OpenGL` |
| Screenshot and CSV to app-specific external storage | met | 605 frames Vulkan, 302 GLES |
| `adb pull` retrieves them without root | met | both pulled into `logs/` |
| `aapt dump badging` shows `android.permission.INTERNET` | met | confirmed on the Vulkan APK |
| Development takes arguments from `UECommandLine.txt` | met | scenario, run length and output directory all logged `source=command line` |
| Shipping takes them from `player.json` with no command line | **not met** | see finding 6 |

The in-app RHI and driver strings, which 4.0b needs for the head unit, are produced:

```
Smoke RHI=Vulkan GPU=PowerVR D-Series DXT-48-1536 MC1 internal_driver=1.4.317|OpenGL ES 3.2 build 25.3@6908880
Smoke RHI=OpenGL GPU=PowerVR D-Series DXT-48-1536        internal_driver=OpenGL ES 3.2 build 25.3@6908880
```

## Findings

Six of these were found only by running on a device. That is the whole argument for doing this task before the primitives rather than after them.

**1. The package shipped its data as a separate OBB.** The engine's default is `bPackageDataInsideApk=false`, so the first device launch went into Unreal's `DownloaderActivity` instead of the game and logged "onPause returning that user quit the download". A sideloaded build has nothing to download from. Fixed by packaging data inside the APK. This would have failed identically on the head unit.

**2. `FPaths::ProjectSavedDir()` is not a usable path on Android.** It returns a virtual path, `../../../UnRealDash/Saved/`, and `ConvertRelativePathToFull` does not resolve it, so the resolver rejected its own defaults for not being absolute. Fixed by asking the platform file layer: `IPlatformFile::ConvertToAbsolutePathForExternalAppForWrite`. The real directory is `/storage/emulated/0/Android/data/com.unrealdash.player/files/UnrealGame/UnRealDash/UnRealDash/Saved/`, with the project name appearing twice, which is correct and not a bug.

**3. Files pushed into `adb`-created directories are unreadable by the application.** Creating `.../files/UnrealGame/UnRealDash/` over `adb` before the app had ever run left those levels owned by `shell` rather than by the app, and the engine's `fopen` of `UECommandLine.txt` failed silently and fell back to the APK's built-in command line. After deleting the tree and letting the app create it, all four levels are owned by `u0_a53` and the same push works. Any contributor will hit this: **launch the app once before pushing anything to it.**

**4. The on-disk command line file replaces the command line, it does not extend it.** `LaunchAndroid.cpp` calls `FCommandLine::Set(TEXT(""))` before appending the file, so a `UECommandLine.txt` that omits `-project=` leaves the engine with no project and it never starts. The file has to carry every argument the APK's own command line supplied.

**5. The GLES screenshot is vertically flipped.** The Vulkan capture is upright; the OpenGL one is upside down, which is the framebuffer origin convention not being corrected in the screenshot readback. The scene on the device is correct; only the captured PNG is flipped. This matters because 4.0b's evidence is screenshots, and any later automated visual comparison would compare a flipped image without saying so.

**6. The Shipping build does not run the scene.** It packages, installs and launches, `libUnreal.so` loads, and the process stays alive, but it never creates its output directory and never exports, well past its configured run length. One earlier attempt died about 1.5 seconds in. Shipping strips logging, so there is no in-app evidence, and the captured logcat in `logs/pixel-shipping/` shows no fatal signal. Unproved and open. The Development path is proved under both RHIs, so nothing downstream is blocked on this except the PLAN 4.0 Shipping clause itself.

**7. Unreal's AndroidFileServer plugin broke the Shipping package.** `:app:ueAFSProjectAssembleDebug` failed. The plugin is disabled: the device workflow here is `adb push` and `adb pull`, which needs nothing from it, and disabling it removes a nested gradle build from every Android package.

**8. The engine adds virtual joysticks on touch platforms.** Observed by the owner watching the device; they do not appear in the captured screenshot. A dashboard has nothing to drive and they would draw over the display. `DefaultTouchInterface=None`.

**9. 33,000 lines of cook artefacts were staged for commit.** `Build/<Platform>/FileOpenOrder/EditorOpenOrder.log` is written by the cook step. Now ignored.

## Cleanup owed when the spike is retired

Marked `SMOKE SPIKE ONLY` in the files themselves:

- `DefaultEngine.ini`: `GameDefaultMap` and `GlobalDefaultGameMode` point at `/Game/Smoke`.
- `DefaultGame.ini`: `+DirectoriesToAlwaysCook=(Path="/Game/Smoke")`.
- `Content/Smoke/`: `M_Smoke` and `L_Smoke`, regenerable with the `MakeSmokeAssets` commandlet.
- `Private/Smoke/` and the `UnRealDashSmokeEditor` module.

Settings that are **not** spike-only and should stay: `bPackageDataInsideApk`, `bUseExternalFilesDir`, `DefaultTouchInterface=None`, and the AndroidFileServer plugin staying disabled.

## Open against PLAN

- PLAN 4.0 says "OpenGL ES 3.2". UE's Android GLES path is the ES3.1 feature level, which is what the GLES package is. The device reports GL ES 3.2, but that does not make the package one.
- PLAN 4.0 says the Shipping build selects "its package" from `player.json`. The spike has no package concept; the loader is task 4.4. The `image` path is the stand-in, and no `package` field was invented.
