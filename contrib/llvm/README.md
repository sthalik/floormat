# LLVM patches

- `lld-icf-mingw-section-suffix-llvm23.patch` applies to LLVM 23.x. Verified
  against LLVM 23.1.6 (`5b10d2d74929`).
- `lld-icf-mingw-section-suffix-llvm24.patch` applies to LLVM 24 development
  sources. Verified against the 2026-10-28 (`9f0c2f993640`) and 2026-11-22
  (`62fd8674914f`) snapshots.
- `libcxx-thread-fp-control-llvm23-24.patch` applies to LLVM 23.x and LLVM 24
  development sources. Verified against all three revisions above.

The ICF patches let LLD merge identical MinGW functions and read-only data
whose section names differ only in their symbol suffix. Floormat enables this
with `--icf=all` in Windows Clang release builds.

The libc++ patch makes Windows x86 and x86-64 threads inherit the creator's
x87 control word and MXCSR, including FTZ/DAZ. It covers libc++ thread
creation; threads created directly through Windows APIs need their own
floating-point setup.

Apply the matching ICF patch and the libc++ patch from the `llvm-project`
source root with `git apply`. Check applicability first with `git apply
--check`. Verification here covers patch application; other LLVM revisions may
need adjustments.
