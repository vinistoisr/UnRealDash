# Architecture rules

These rules bind every contribution, human or agent. They exist so that Stage 0 code does not have to be rewritten when Stage 1 adds the 3D schema, the command channel, package staging, and the plugin host. PLAN.md defines what is built and when; this file defines the shape it must keep.

## Layers and dependency direction

```
runtime/UnRealDash (Unreal player, UMG, scene, connectors' platform halves)
        |  may include
        v
packages/dashboard-spec (document model, schema validation, package reader)
        |  may include
        v
runtime/UnRealDash/Source/SignalCore (signal-core: samples, registry, rules, replay, parsers)
        |  may include
        v
      C++20 standard library only
```

- Dependencies point downward only. `SignalCore` includes nothing from `dashboard-spec` or the engine. `dashboard-spec` includes `SignalCore` headers and its vendored JSON libraries, nothing from the engine. The engine modules include both.
- `SignalCore` source files contain no Unreal headers, no Unreal types, no engine macros. The single exception is the module-registration file, which the CMake build excludes. The CMake build in `packages/signal-core` is what enforces this: it compiles the same files with no engine on the path.
- Python tools under `tools/` depend on `packages/dashboard-spec/schema/` and `tests/fixtures/` only. They never import from the runtime tree.
- No layer reaches upward through a callback that carries an upper-layer type. Callbacks cross layers as plain function objects over lower-layer types.

## Module shape

- One responsibility per module, one public header per concept, and a `README.md` in every top-level directory stating what belongs there and what does not.
- Public headers under `Public/`, everything else under `Private/`. A public header exposes the smallest surface that the next layer needs. Nothing in `Private/` is included from another module.
- No global mutable state. No singletons. Anything that needs a clock, a logger, a random source, or a file system takes it as a constructor argument, so tests can inject fakes. The monotonic clock is always injected.
- Errors are values. `SignalCore` and `dashboard-spec` never throw and are compiled with exceptions and RTTI off. Fallible operations return a result type or an error code with a stable enumerated value and a human-readable message that names the offending input (file, field, JSON pointer, byte offset).
- Data crosses module boundaries as plain structs, spans, and callbacks. No standard-library containers in a public API that the engine consumes; no engine containers in a public API that the CMake build consumes.

## Extension points that must survive Stage 1

These are the seams later stages will widen. Do not collapse them for convenience.

- **Connector interface.** Transport, session, decoder, and signal mapping are separate objects with separate lifetimes, per `docs/CONNECTORS.md`. A new transport must not require touching a decoder; a new decoder must not require touching a transport. The definition-pack loader is the only way a decoder learns a frame layout.
- **Component registry.** The runtime builds UI from the document through a registry keyed by component type name. Adding a primitive means registering one builder; nothing switches on a type string outside that registry.
- **Document model.** Component IDs are stable and opaque. Bindings, rules, and theme tokens are separate sections that reference components by ID. Nothing assumes a component's position in the tree.
- **Rule engine.** Operations are a whitelist table; adding an operation adds one entry and one test. The evaluator never parses text.
- **Recording format.** Versioned with a header. Readers accept older versions; writers emit the current one.
- **Package reader.** Reads a `.udash` archive and an unpacked directory through one interface with one set of bounds and one path resolver. Staging and rollback in Stage 1 wrap this reader; they do not replace it.
- **Metrics events.** Every record type carries a schema version. New record types are added, existing columns are never renamed.

## Testing shape

- Every module has tests that run with no engine installed. Unit tests live next to the code they test under `tests/` in the same package and use doctest (C++) or pytest (Python).
- Fixtures are data, not code: synthetic signal recordings, frame byte streams, valid and invalid documents, each with an `.expected` sibling where the expected failure matters. Fixtures under `tests/fixtures/` are shared by every validator and every language.
- A gate in PLAN.md is a command that exits non-zero on failure. Scripts under `scripts/` wrap those commands so that CI and a contributor's laptop run the same thing.

## Style

- C++20 for the engine-independent code; the engine's own standard inside engine modules. `clang-format` and `clang-tidy` configuration files at the repo root, applied to the engine-independent code; the engine tree follows Epic's conventions.
- Names say what a thing is, not how it is implemented. No abbreviations that need a glossary.
- Comments explain why, not what. A comment that restates the code is removed.
- Plain factual language in every document and every string the user can see. No em dashes. No marketing words.
- Commits are small, describe one change, and name the PLAN.md task they serve.

## What is not allowed

- Hand-authored widget Blueprints for the control dashboard. The document is the only source of layout.
- Parsing the owner's third-party schema XML at runtime. The offline converter emits a definition pack; the player reads the pack.
- Text expression grammars. Rules are a JSON expression tree.
- Zero as a stand-in for missing data. Quality is explicit.
- Copying code between `SignalCore` and `dashboard-spec` to avoid a dependency. The dependency direction above is the fix.
- Silent fallbacks. A degraded path logs that it is degraded and the report says so.
