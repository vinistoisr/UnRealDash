# Stage 0 feasibility

Skeleton written by chunk 04; completed by PLAN.md 7.1.

## Stage 0 exit conditions

PLAN.md 7.1 records evidence and status for the six conditions in PLAN.md Goal.

1. A packaged player renders two dashboards on Windows x64 and on the reference Android device: a control dashboard defined entirely by JSON, and a hand-authored showcase scene.
   Status: not yet assessed. [Evidence]()
2. A Linux x86-64 package builds from the Windows workstation.
   Status: not yet assessed. [Evidence]()
3. Editing `dashboard.json` changes the control dashboard and editing `showcase.json` changes the showcase, in both cases without rebuilding or repackaging the player.
   Status: not yet assessed. [Evidence]()
4. The disconnect scenario marks affected instruments stale within the configured per-signal deadline, and no instrument continues animating fabricated values.
   Status: not yet assessed. [Evidence]()
5. The metrics table in PROJECT-PLAN section 8 is filled with measured numbers, recorded with build id, device, resolution, power mode, temperature and sample size, covering both the Vulkan and the OpenGL ES 3.2 rendering paths on the head unit, a 60-minute thermal run on the head unit, an 8-hour desktop soak on Windows, and resident memory sampled over both runs.
   Status: not yet assessed. [Evidence]()
6. `docs/reports/stage0-feasibility.md` exists and carries an explicit go/no-go recommendation on Unreal for this product.
   Status: not yet assessed. [Evidence]()

## Metrics table

PLAN.md 6.5 supplies measurements and run context; PLAN.md 7.1 records them here.

| Build id | Device | Resolution | Power mode | Active RHI | Driver string | Temperature | Temperature source | Sample size | Scenario | Dashboard | Effect settings | Frame time p95 | Frame time p99 | Missed-frame count | Receive-to-present p95 (excludes scanout) | Receive-to-present observation count | Adapter polling delay | Expiry-to-present | Fired-not-displayed count | Unresolved expiry count | Signal | Published count | Presented count | Presentation coverage (presented / published) | Acquired-not-displayed count | Superseded-unacquired count | Latest-unacquired count | Launch timing endpoint | Launch-to-first-usable-value | Launch timing uncertainty | Android am start -W durations | Android launch state | Launch timing disagreement | OS boot time | Resident memory trend | Resident memory peak | Resident memory final | Resident memory slope confidence interval | Texture memory trend | Thermal run duration | Desktop soak duration | Loss of telemetry | Update failure |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| not measured (blocked by 6.5) | not measured (blocked by 6.5) | not measured (blocked by 6.5) | not measured (blocked by 6.5) | not measured (blocked by 6.5) | not measured (blocked by 6.5) | not measured (blocked by 6.5) | not measured (blocked by 6.5) | not measured (blocked by 6.5) | not measured (blocked by 6.5) | not measured (blocked by 6.5) | not measured (blocked by 6.5) | not measured (blocked by 6.5) | not measured (blocked by 6.5) | not measured (blocked by 6.5) | not measured (blocked by 6.5) | not measured (blocked by 6.5) | not measured (blocked by 6.5) | not measured (blocked by 6.5) | not measured (blocked by 6.5) | not measured (blocked by 6.5) | not measured (blocked by 6.5) | not measured (blocked by 6.5) | not measured (blocked by 6.5) | not measured (blocked by 6.5) | not measured (blocked by 6.5) | not measured (blocked by 6.5) | not measured (blocked by 6.5) | not measured (blocked by 6.5) | not measured (blocked by 6.5) | not measured (blocked by 6.5) | not measured (blocked by 6.5) | not measured (blocked by 6.5) | not measured (blocked by 6.5) | not measured (blocked by 6.5) | not measured (blocked by 6.5) | not measured (blocked by 6.5) | not measured (blocked by 6.5) | not measured (blocked by 6.5) | not measured (blocked by 6.5) | not measured (blocked by 6.5) | not measured (blocked by 6.5) | not measured (blocked by 6.5) | not measured (blocked by 6.5) |

## CI from a clean clone (task 1.7)

Recorded 2026-09-16 (2026-09-17 02:07 UTC) on the first push of the workflows, commit 21e1e59, hosted runners with no cached dependencies and no local state.

- signal-core.yml run 35173217954: build (ubuntu-latest) success, build (windows-latest) success, tsan success. Both build jobs printed doctest assertions 9,236 passed, 0 failed. The ThreadSanitizer job ran the 2.13 threading stress test with no ThreadSanitizer warning in its log.
- spec-tools.yml run 35173217895: python-tools success, cpp-validator (windows-latest) success, cpp-validator (ubuntu-latest) success, parity success. The Python validator reported valid=27 invalid=65 mismatches=0 on Python 3.12; the parity job reported 0 differing lines.
- Every checkout ran with lfs: false and the LFS-absence step passed on every job (the probe PNG is a pointer in the checkout, .git/lfs/objects empty).

## Findings

PLAN.md 7.1 fills this section with findings.

## What failed

PLAN.md 7.1 fills this section with what failed.

## What never ran

PLAN.md 7.1 fills this section with what never ran.

## Go or no-go recommendation

PLAN.md 7.2 owns the recommendation.
