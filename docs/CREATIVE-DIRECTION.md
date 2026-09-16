# Creative direction

## Product promise

Build the dashboard you can imagine: dimensional instruments, expressive materials, coordinated motion, and live vehicle visualizations, authored through conversation and owned by the user.

Advanced graphics are a reason to choose the platform. A thin in-car player should be a well-optimized real-time presentation engine. It should not carry the AI authoring system, and it should not reduce every design to flat gauges.

These are proposed experiences to prototype, not currently implemented features.

## Signature experiences

| Experience | What the user sees | What the system needs |
| --- | --- | --- |
| Dimensional instruments | Layered dial rings, physical-looking needles, recessed markings, subtle parallax | Mesh hierarchy, controlled camera, crisp text overlay, bounded transforms |
| Reactive materials | A luminous RPM arc that changes texture and energy with load; temperature expressed through surface color | Signal-driven material parameters, authored ranges and quality variants |
| Spatial mode changes | Instruments move and reshape together when switching between road, track, and diagnostic views | State machine, shared-element transitions, interruption and reduced-motion policies |
| Live vehicle view | Wheel steering, open doors, tire temperatures or energy flow shown on a 3D vehicle | Prepared model, semantic part IDs, verified signals and explicit unavailable states |
| Rich data history | A depth-layered boost/RPM trace or animated session comparison | Bounded history buffers, graph primitives and replay controls |
| Cinematic presentation | A short startup reveal or parked showcase with particles and camera motion | Timelines, effect budgets, instant skip; essential values available without waiting for animation |

Day/night appearance should change materials and lighting coherently, not simply dim a screenshot. Rich visuals must retain legible values. Blur, bloom, glass effects, translucency, particle density, and scene capture resolution are explicit quality controls, not unlimited decorations.

## Reference reconstruction as an entry point

Users should be able to say “Recreate the dashboard in these screenshots” and then “Give it depth, animate the rings, and add a live vehicle view.”

1. Accept screenshots, source artwork, descriptions, and eventually short reference videos. Extract video keyframes and motion descriptions on the authoring host when the chosen model does not accept video directly.
2. Identify instruments, labels, proportions, typography, colors, and visual layers. Produce an editable component tree rather than a single flattened image.
3. Label uncertain interpretations: a screenshot cannot reveal hidden pages, signal identities, warning thresholds, or transition behavior. Request missing information or use clearly marked provisional settings.
4. Bind components to an independently supplied signal catalog. A displayed number does not identify its underlying vehicle source.
5. Compare the reconstruction against its reference at matching resolution and representative states. Allow an overlay/opacity comparison and per-element refinement.
6. Offer an explicit enhancement pass. Preserve familiar placement and information hierarchy unless the user asks to change them; add new depth, effects, interactions, or transitions as separate revisions.
7. Export the editable source, permitted assets, and runtime package. Track asset provenance and usage rights; do not assume reference screenshots grant redistribution rights to every depicted asset.

Direct project-file import is an additional adapter capability where formats and permissions allow it. Reference reconstruction must work without such an importer. Pixel-perfect reconstruction cannot be guaranteed when source fonts, artwork, geometry, or hidden behavior are unavailable.

Public documentation should use neutral phrases such as reference reconstruction, existing dashboard, and compatibility adapter. Do not name competing dashboard products, link their domains, or make unverified claims about their architectures in the public repository.

## First showcase: dimensional performance cockpit

Use an original generic design and synthetic telemetry so the demo is redistributable and reproducible.

The resting view contains two layered 3D instruments around a central speed/gear display. A luminous band responds to RPM. A small generic vehicle silhouette/mesh presents available subsystem state. Fine text and important indicators remain legible in an overlay.

Switching mode animates the same instruments into a compact arrangement and reveals a history graph. Day/night transitions modify the palette, lighting, and material response together. A brief startup reveal is skippable and never delays essential data. A disconnect scenario visibly marks stale instruments; it must not continue animating synthetic “live” readings.

Prompt-edit examples for the acceptance suite:

- “Give the outer ring more depth without changing its scale markings.”
- “Make the illuminated band react to RPM, but leave temperature colors unchanged.”
- “Transition to the diagnostic layout over 400 ms and keep speed visible throughout.”
- “Rebuild this screenshot as editable instruments, then show an enhanced version separately.”
- “Reduce effects for this tablet while retaining the layout and all warnings.”

These examples require semantic targets, parameter limits, stable IDs, and deterministic behavior. Free-form generated Unreal code is an advanced development path, not the only way to make compelling designs.

## Quality and evaluation

Build the simple instrument control scene and the showcase together. The control measures engine/runtime overhead; the showcase tests whether the product justifies choosing Unreal.

Mobile, desktop, and showcase profiles share the design's structure and signal meanings, but can vary shading, particles, reflection method, antialiasing, and geometry complexity. Show the user a preview of any downgrade. Do not silently remove an indicator or change what a color means to satisfy a performance budget.

Evaluate reference fidelity, edit locality, temporal smoothness, readable text, depth/occlusion correctness, resource use, and behavior with stale inputs. Capture fixed-camera frames at known scenario timestamps for regression comparison. Include transition interruption, repeated mode switching, and thermal throttling in the acceptance runs.

The product goal is sophisticated creative freedom with predictable runtime behavior. Both the visual experience and sustained performance must pass the first feasibility gate.
