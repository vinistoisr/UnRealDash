# UnRealDash

An AI-driven automotive dashboard project built around Unreal Engine, with community ownership, extensibility, and portable dashboard designs as core goals.

## Project status

Initial planning. No dashboard runtime or Unreal project has been implemented yet.

## Direction

- Create and edit dashboards through prompts, reference images, and text descriptions.
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

Render one externally defined dashboard with simulated telemetry in a packaged Unreal application. Measure startup time, memory use, frame rate, and sustained temperature on representative hardware, and verify layout updates without rebuilding the player.

## Licensing

An open-source license for the project's original code has not yet been selected. Unreal Engine and third-party assets remain subject to their respective licenses; this repository does not include Unreal Engine source code.
