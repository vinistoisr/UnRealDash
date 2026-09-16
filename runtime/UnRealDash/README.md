# Unreal player

The Unreal project, rendering, scene, platform connector halves and the canonical
SignalCore sources belong here (PLAN.md 4.x and 5.x). No project exists yet.
SignalCore must remain plain C++20 without Unreal headers or types, apart from its
module-registration file excluded by CMake. Offline Python tools and duplicate
signal implementations do not belong here.
