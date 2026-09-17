#pragma once
// Compiler linkage annotations only; no engine header or engine macro is required.
#if defined(_WIN32) && defined(DASHBOARD_SPEC_SHARED)
#if defined(DASHBOARD_SPEC_BUILD)
#define DS_EXPORT __declspec(dllexport)
#else
#define DS_EXPORT __declspec(dllimport)
#endif
#else
#define DS_EXPORT
#endif
