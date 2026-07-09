---
name: syscall-implementation
description: |
   Guide for adding a new kernel syscall end-to-end. Use when asked to implement or register kernel syscalls, add OS wrappers, and assembly stubs.
   Triggers: syscall, SWI, IOSC, syscall table, wrapper, kernel API, debugging, wiring failures
license: MIT
keywords:
   - syscall
   - swi
   - iosc
   - syscall-table
   - wrapper
   - kernel-api
   - debugging
   - wiring-failures
---

# Syscall Implementation Guide

Requires changes to **5 files**.

## The 5 Files

| Step | File | Action |
|------|------|--------|
| 1 | `kernel/source/crypto/iosc.c` | Implement kernel function (`IOSC_*`) |
| 2 | `kernel/source/crypto/iosc.h` | Declare kernel function |
| 3 | `kernel/source/interrupt/syscall.cpp` | Register in syscall table (`SYSCALL(...)`) |
| 4 | `core/include/ios/syscalls.h` | Declare OS wrapper (`OSIOSC*`) before `#pragma GCC pop_options` |
| 5 | `core/source/ios/syscalls_asm.s` | Add SWI stub (`_SYSCALL OSIOSC*`) |

## Step-by-step workflow

1. Implement kernel function
   - Add `s32 IOSC_MyFunction(...);` implementation in `kernel/source/crypto/iosc.c`.
   - For IOSC-style functions use `IOSC_BEGIN_SAFETY_WRAPPER(ret, keyRet)` / `IOSC_END_SAFETY_WRAPPER(...)`.
   - Keep behavior-preserving; prefer a small inner helper `_IOSC_MyFunctionInner(...)`.

2. Add kernel header declaration
   - Add `s32 IOSC_MyFunction(...);` inside `#ifndef MIOS` in `kernel/source/crypto/iosc.h`.

3. Register syscall in table
   - In `kernel/source/interrupt/syscall.cpp` replace the `SYSCALL_NULL` at the desired numeric index with `SYSCALL(IOSC_MyFunction)`.
   - Ensure the header is `#include`d at top of the file.

4. Add OS wrapper declaration
   - Add `s32 OSIOSCMyFunction(...);` to `core/include/ios/syscalls.h` **before** the final `#pragma GCC pop_options`.
   - Project convention: wrapper declarations must be placed before `#pragma GCC pop_options`.

5. Add assembly stub
   - In `core/source/ios/syscalls_asm.s` add `_SYSCALL OSIOSCMyFunction, 0x00NN` in numeric order.

6. Build and smoke test
   - From repository root run:

```bash
make -C core
make -C modules/es
```

Verify linkage and run a basic functional smoke test for the new syscall.

## Naming conventions (must not be renamed or normalized)
- Kernel implementations: `IOSC_*`
- OS-visible wrappers: `OSIOSC*`
- Syscall table macro: `SYSCALL(...)` in C++ table
- Assembly SWI macro: `_SYSCALL` in `syscalls_asm.s`

## Completion Checklist (matches the 5-file pipeline)
- [ ] `IOSC_MyFunction` implemented and compiles
- [ ] Declaration present in `kernel/source/crypto/iosc.h` under `#ifndef MIOS`
- [ ] `SYSCALL(IOSC_MyFunction)` added at the correct index
- [ ] `OSIOSCMyFunction` declared in `core/include/ios/syscalls.h` before `#pragma GCC pop_options`
- [ ] `_SYSCALL OSIOSCMyFunction, 0x00NN` added to `core/source/ios/syscalls_asm.s`
- [ ] Rebuilt `core` and module; no link errors
- [ ] Confirm symbols and numeric index match RE/Ghidra
- [ ] Run functional smoke test

## Companion docs (kept out of `SKILL.md`)
- `examples.md`, `patterns.md`, `troubleshooting.md` (located in same skill directory)
