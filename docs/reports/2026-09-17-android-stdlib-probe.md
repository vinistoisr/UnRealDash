# Android standard-library file API probe

Date: 2026-09-17. Device: Pixel 10 Pro, Vulkan, Development build. PLAN 4.4 de-risking.

## Question

`dashboard-spec` reads every file through one function, `ReadBounded` in `BoundedParse.cpp:10`,
which uses `std::ifstream` over a `std::filesystem::path`. Its package traversal, symlink,
hard-link and case-collision rejections are built on `std::filesystem::symlink_status`,
`hard_link_count` and `canonical`. Forty-three fixtures currently prove those rejections.

If any of that is inert under Epic's Android toolchain, porting the library into the engine needs
a different plan, and `IPlatformFile` cannot substitute because it exposes neither hard-link
counts nor symlink status. Finding out during the port would be the expensive way to learn it.

## Method

A probe in the 4.0 smoke spike HUD, run at `BeginPlay` against a real file pushed to the device
at the app-specific external storage path, and against the directory containing it. Not a
synthetic path: the same file the spike already loads as a texture.

## Result

Every call succeeded with `error=0`.

```
probe ifstream open=true read_bytes=4096 path=/storage/emulated/0/Android/data/com.unrealdash.player/files/UnrealGame/UnRealDash/UnRealDash/Saved/smoke.png
probe is_directory=true error=0
probe symlink_status is_symlink=false error=0
probe hard_link_count=1 error=0
probe canonical error=0 path=/storage/emulated/0/Android/data/com.unrealdash.player/files/UnrealGame/UnRealDash/UnRealDash/Saved/smoke.png
probe file_size=94346 error=0
probe recursive_directory_iterator entries=32 error=0
```

`file_size` matches the pushed file exactly: `adb push` reported 94346 bytes.

## Conclusion

The directory form of the package reader is viable on Android as written. `hard_link_count` and
`canonical` were the two most likely to be inert, and both work. PLAN 4.4 needs no Android
fallback for the directory form, and `ReadBounded` must not be rerouted through `IPlatformFile`,
which would put an Unreal type inside a library the CMake build compiles with no engine present.

## What this does not answer

- Whether the whole library **links** for Android ARM64. One probe exercises a few symbols; the
  port pulls in rapidjson, valijson and miniz as well. Chunk 10 still builds the module for
  Android first.
- Where `StagedFileType.NonUFS` files actually land, and whether the path the player resolves at
  runtime matches. That is a staging question, not a standard-library question, and chunk 10
  deliverable 1a covers it.
- Anything about files inside the `.pak`. `std::ifstream` cannot read those at all, which is why
  the schema files are staged as non-UFS.
