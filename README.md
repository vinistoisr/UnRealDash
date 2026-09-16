# UnRealDash

A next-generation automotive dashboard platform built around Unreal Engine: AI-authored instruments, expressive animation, reactive materials, and real-time 3D, with community ownership and portable designs as core goals.

## Project status

Initial planning. No dashboard runtime or Unreal project has been implemented yet.

## Planning documents

- [Project plan](docs/PROJECT-PLAN.md): proposed product experience, architecture, package and signal design, AI workflow, performance gates, and roadmap.
- [Creative direction](docs/CREATIVE-DIRECTION.md): signature experiences, reference reconstruction, and the first showcase dashboard.
- [Connector architecture](docs/CONNECTORS.md): Android USB, Bluetooth OBD-II, aftermarket ECU plugins, and compatibility testing.
- [Plugin authoring experience](docs/PLUGIN-EXPERIENCE.md): simple definition packs, proposed custom-code SDK, examples and contributor acceptance criteria.
- [Custom test rig](docs/PILOT-INTEGRATION.md): the existing Scirocco telemetry path for field testing, separate from generic OBD-II onboarding.
- [Research notes](docs/RESEARCH.md): primary sources, findings, and unresolved assumptions.

## Direction

- Create and edit dashboards through prompts, reference images, and text descriptions.
- Reconstruct editable dashboards from user-supplied references, then evolve them with depth, motion, materials, and live data.
- Author on a separate workstation or through a web interface.
- Deploy to a minimal in-car player that reads telemetry, evaluates deterministic rules, and renders dashboards locally without an AI or cloud dependency.
- Target Android, Windows, and x86-64 Linux, with hardware compatibility and performance validated through prototypes.
- Keep telemetry adapters and the dashboard specification independent of Unreal where practical.
- Support replaceable AI providers, exportable project files, and community-developed connectors.

## Proposed architecture

1. **Authoring system:** prompting, asset generation, previews, validation, and platform builds.
2. **Dashboard package:** versioned layouts, assets, signal bindings, and animation parameters.
3. **In-car player:** Unreal rendering, telemetry integration, interaction, and local dashboard storage with rollback.

Dashboard changes using existing player capabilities should not require an application rebuild. New native connectors or executable capabilities require a platform build and deployment.

## First milestone

Build a packaged Unreal feasibility demo with both a simple control dashboard and an expressive showcase: layered 3D instruments, reactive materials, and coordinated transitions driven by simulated telemetry. Measure startup time, memory, frame rate, and temperature on representative hardware, and verify supported layout and animation edits without rebuilding the player.

## Licensing

An open-source license for the project's original code has not yet been selected. Unreal Engine and third-party assets remain subject to their respective licenses; this repository does not include Unreal Engine source code.
