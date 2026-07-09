# Implementation Patterns & Helpers

This file collects common patterns, helpers, and important implementation notes referenced from `SKILL.md`.

## IOSC helper checks

- `IOSC_CheckCurrentProcessOwnsKey(u32 handle)` — verify caller owns keyslot.
- `IOSC_CheckCurrentProcessCanRead(const void* p, u32 size)` — memory read check.
- `IOSC_CheckCurrentProcessCanReadWrite(const void* p, u32 size)` — read/write check.

## Safety wrapper pattern

Use the `IOSC_BEGIN_SAFETY_WRAPPER(ret, keyRet)` / `IOSC_END_SAFETY_WRAPPER(ret, keyRet)` macros
for functions that access IOSC keyslots or global keyring state. The wrapper handles mutex/swap logic.

## Message queue alignment (related concerns)

- IPC message queues must be aligned for DMA/cache coherency (e.g., `ALIGNED(0x20)`).

## Project conventions

- Place OS wrapper declarations in `core/include/ios/syscalls.h` BEFORE the `#pragma GCC pop_options` line.
- Keep `IOSC_*` for kernel implementations and `OSIOSC*` for OS-visible wrappers.
- Use `SYSCALL(...)` entries in `kernel/source/interrupt/syscall.cpp` for syscall table wiring.
