# The Shipping build finding, narrowed

Date: 2026-09-18. Follows finding 6 in `docs/reports/2026-09-17-smoke-spike.md`, which recorded that
the Android Shipping build packages, installs and launches but never exports, and left it open.

## It is not Android specific

A Windows Shipping package reproduces it. That is the useful part: the bug moves from something only
reachable over `adb` on a device to something debuggable at the desk.

## The original description is wrong in a way that matters

Finding 6 says the Shipping build "does not run the scene". It does.

Measured on the real binary, `UnRealDash/Binaries/Win64/UnRealDash-Win64-Shipping.exe`:

```
alive: True   window: 'UnRealDash'   threads: 90   memory: 787 MB
```

That is a fully initialised engine with a window. The scene runs. What never happens is the
**export**: no `metrics.csv` and no screenshot, in the configured `output_directory` or anywhere
else on disk.

## The measurement trap that produced the wrong description

`UnRealDash.exe` at the root of the package is a launcher stub, not the game. Running and measuring
that gives:

```
alive: True   window: ''   threads: 4   memory: 7 MB
```

which looks exactly like an engine that never started. Anyone diagnosing this from the stub will
conclude the scene never runs. Measure `Binaries/Win64/UnRealDash-Win64-Shipping.exe`.

## Ruled out

- **Config source.** Fails identically whether configured from `player.json` with no command line or
  from an explicit command line.
- **Game mode.** Chunk 10 changed `GlobalDefaultGameMode` to the package loader, so a plain launch no
  longer runs the smoke HUD at all. An early attempt at this diagnosis missed that and proved
  nothing. Requesting `?game=/Script/UnRealDash.SmokeGameMode` explicitly still does not export.
- **Missing content.** `M_Smoke`, `L_Smoke` and the engine fonts are all in the Shipping pak, and the
  `EngineFonts` list is byte for byte the same as the Development Android package.

## Still open

Where in the export path it stops. The candidates are that `ASmokeHUD::BeginPlay` returns early and
leaves `bReady` false, that `DrawHUD` never reaches the run-length branch, or that the branch runs
and the write fails.

Logging cannot answer it: `UnRealDash.Target.cs` explains why Shipping logging is not enabled, and
that reasoning still holds, since a Launcher engine install cannot supply the build environment it
needs. The next step is file-based tracing, which works in Shipping without logging: append a line
to a fixed path at module start, game mode construction, `BeginPlay` entry, each early return,
config resolution, first `DrawHUD`, and the export branch. That is one code change and one package.

Nothing downstream is blocked on this. The Development path is proved under both RHIs.
