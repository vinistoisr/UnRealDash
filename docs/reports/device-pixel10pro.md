# Desk device: Pixel 10 Pro

Recorded 2026-09-17 over USB ADB, for PLAN.md task 4.0. This is the desk device the smoke spike runs on. It is not the qualifying device: the head unit is, and 4.0b is what covers it. Nothing measured here says anything about the head unit's Mali-G57 driver.

| Fact | Value | How it was read |
| --- | --- | --- |
| Serial | 56251FDCH0009W | `adb devices -l` |
| Model | Pixel 10 Pro (`blazer`) | `adb devices -l` |
| Build fingerprint | `google/blazer/blazer:17/CP2A.260805.005.A1/15828182:user/release-keys` | `getprop ro.build.fingerprint` |
| Android release | 17 | `getprop ro.build.version.release` |
| SDK level | 37 | `getprop ro.build.version.sdk` |
| CPU ABI | arm64-v8a | `getprop ro.product.cpu.abi` |
| GPU | Imagination Technologies PowerVR D-Series DXT-48-1536 | `dumpsys SurfaceFlinger` |
| OpenGL ES | 3.2, driver build 25.3@6908880 | `dumpsys SurfaceFlinger` |
| Vulkan | 1.4.0 (`android.hardware.vulkan.version=4210688`), level 1, compute | `pm list features` |
| Memory page size | 4096 bytes | `getconf PAGE_SIZE` |
| Total RAM | 15,949,068 kB, about 15.2 GiB | `/proc/meminfo` |
| Panel | 1280 x 2856, density 480 | `wm size`, `wm density` |

## What these change

**Android 17, SDK 37.** PLAN A1 and task 4.0 describe this device as Android 16. It is Android 17. The project targets SDK 35 and requires a minimum of 26, both of which this device accepts, so nothing is blocked; the assumption is simply out of date and is corrected here rather than in passing.

**Vulkan 1.4, not 1.3.** PLAN 4.0 says "Vulkan 1.3 or later", which holds. Recorded exactly because the head unit is the opposite case, a device pinned at Vulkan 1.1, and the gap between the two is the whole reason the spike runs on both.

**The GPU is Imagination, as PLAN assumed.** A PowerVR D-Series part. This matters only as a reminder of what the desk run does not prove: it exercises packaging, files, arguments and export on a driver stack that shares nothing with the head unit's Mali-G57.

**OpenGL ES 3.2 is present on the device.** UE's Android GLES path is the ES3.1 feature level, selected by `bBuildForES31`, which is not the same thing as PLAN 4.0's wording of "OpenGL ES 3.2". The device supporting 3.2 does not make the UE package an ES 3.2 package. The discrepancy is recorded in `docs/build/chunk-07-smoke-spike.md` and stays open against PLAN.

**Memory page size is 4096.** Worth recording because it is a failure this spike could plausibly have found and did not. Android 15 introduced 16 KB page support and Android 16 began requiring it of applications targeting SDK 36 or newer; a native library that is not aligned for 16 KB pages fails to load on such a device. This device runs 4 KB pages and the project targets SDK 35, so neither condition applies here. It is not evidence that the engine's prebuilt libraries would survive a 16 KB device.

## App-specific external storage is reachable without root

Checked ahead of the package existing, because the whole export workflow in PLAN 4.0 depends on it and finding out after the spike was built would be the expensive order. With no application installed:

```
adb shell mkdir -p /sdcard/Android/data/com.unrealdash.player/files   # rc 0
adb push probe.txt /sdcard/Android/data/com.unrealdash.player/files/  # 1 file pushed
adb shell ls -l  /sdcard/Android/data/com.unrealdash.player/files     # -rw-rw-rw- shell ext_data_rw 6 probe.txt
adb pull /sdcard/Android/data/com.unrealdash.player/files/probe.txt   # 1 file pulled, contents match
```

So Android 17 does not block shell access to this path, and no runtime permission or root is involved. The probe directory was removed afterwards, because it was created owned by `shell` rather than by the application, and a wrongly owned directory left in place could produce a write failure at first launch that looks like an engine fault.

## Not yet recorded

Whether the engine's `GFilePathBase` actually resolves into that directory once `bUseExternalFilesDir` is set, and whether the application can write there itself. Both need the package installed, which is chunk 07's work.
