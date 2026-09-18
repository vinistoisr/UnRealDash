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

## Narrowed again: no HUD runs at all

File-based tracing was added to `ASmokeHUD`, one line per milestone including every early return,
written with `FFileHelper` so it works where `UE_LOG` is compiled out. The instrumented Shipping
binary was verified to contain the trace strings, searching as UTF-16 because `TEXT()` literals are
wide and an ASCII search finds nothing and looks like a missing build.

**The trace file is never created.** Not at the resolved Saved path, not at the project-relative
fallback, nowhere on disk. So `ASmokeHUD::BeginPlay` does not run, which rules out every early
return inside it and the whole export path below it.

It is not specific to that HUD. Launching the same Shipping build with no map URL, so chunk 10's
`GlobalDefaultGameMode` package loader is active, and passing `-udash=` with a package that is known
good, produces a window that is entirely black. The package screen renders neither the document nor
its on-screen error, and that screen is the one thing in this project guaranteed to draw something
in every case.

So the failure is above the HUD: in Shipping, the engine initialises and opens a window, and then no
HUD or UMG surface is created or drawn.

## Still open

Why no HUD is created. The next step is tracing above this layer rather than inside it: game
instance start, map load completion, game mode construction and HUD class instantiation. The same
file-based technique works there.

Superseded: the earlier note that the candidates were inside `BeginPlay`, `DrawHUD` or the write
itself. All three are excluded by the trace never appearing.

Previously the open question was where in the export path it stops. The candidates are that `ASmokeHUD::BeginPlay` returns early and
leaves `bReady` false, that `DrawHUD` never reaches the run-length branch, or that the branch runs
and the write fails.

Logging cannot answer it: `UnRealDash.Target.cs` explains why Shipping logging is not enabled, and
that reasoning still holds, since a Launcher engine install cannot supply the build environment it
needs. The next step is file-based tracing, which works in Shipping without logging: append a line
to a fixed path at module start, game mode construction, `BeginPlay` entry, each early return,
config resolution, first `DrawHUD`, and the export branch. That is one code change and one package.

Nothing downstream is blocked on this. The Development path is proved under both RHIs.
