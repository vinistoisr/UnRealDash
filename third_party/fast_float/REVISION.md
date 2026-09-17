# fast_float

Upstream: https://github.com/fastfloat/fast_float
Revision: release 8.3.0, single-header bundle `fast_float.h` from the release assets
Retrieved: 2026-09-17
Files: fast_float.h (unmodified), LICENSE-MIT
Licence: Apache-2.0 OR MIT OR BSL-1.0 (MIT text kept here)

Why it is vendored: the recording reader needs a correctly rounded decimal to binary64
conversion, and `std::from_chars` for floating point is not available on every toolchain
this project builds with. The Android NDK r27 libc++ declares that overload deleted, while
the Linux toolchain's clang 20 and MSVC provide it. Falling back per platform would mean
two conversion paths that can disagree in the last unit in the last place, which would
break the byte-identical recording round trip in PLAN.md task 2.7. This header is the same
Eisel-Lemire implementation newer libc++ releases use, so every target now runs identical
code and Windows and Linux results are unchanged.
