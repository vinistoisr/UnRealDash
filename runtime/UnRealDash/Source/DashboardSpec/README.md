# DashboardSpec (PLAN 4.4)

This directory owns the sole engine-independent C++ source copy for dashboard
validation and package loading. `packages/dashboard-spec` holds the CMake build,
schemas, tests and CLI tools. Both build systems compile these sources with
exceptions and RTTI disabled. Only `Private/DashboardSpecModule.cpp` includes an
engine header. `Public/dashboard_spec/Export.h` uses compiler linkage annotations
for modular Windows builds and has no Unreal dependency.

The portable reader deliberately retains standard-library filesystem checks,
including symlink status, canonical paths and hard-link counts. Replacing these
with Unreal's platform filesystem would weaken package security.
