# StarStruck Build System

## Toolchain
- **DevkitARM** — arm-none-eabi gcc (must be in `$DEVKITARM`)
- **Target CPU**: ARM926EJ-S (`-mcpu=arm926ej-s -mbig-endian`)
- **Thumb**: All modules compiled as Thumb (`-mthumb` via `sdk/starstruck_rules`)
- **Kernel**: ARM mode (no `-mthumb`)
- **Optimisation**: `-Os`, with `-ffunction-sections`, `-fomit-frame-pointer`
- **Warnings**: `-Wall -Wextra -Wpointer-arith -Wconversion`
- **Spec file**: `sdk/starlet.specs` (used by default via `SPECSFLAGS`)

## Key Compile Flags
| Flag | Purpose |
|------|---------|
| `-mbig-endian` | BE ARM (Hollywood Starlet) |
| `-mcpu=arm926ej-s` | Target CPU |
| `-mthumb` | Module code = Thumb |
| `-Os` | Optimise for size |
| `-fno-asynchronous-unwind-tables` | Payloads only |
| `-fpic` | Payloads only |
| `-nostartfiles -nodefaultlibs` | No libc startup |
| `-n` | No page alignment in output |
| `-DMIOS` | Conditional MIOS-mode (skips most syscalls) |

## SDK File Layout
```
sdk/
  wii_rules          # Base rules: ARCH, CFLAGS, LDFLAGS, toolchain vars, %.elf and %.a recipes
  starstruck_rules   # Includes wii_rules, adds -mthumb
  starlet.specs      # GCC spec file
  modules.mk         # Module build template (auto-discovers .c/.cpp/.S files)
  payload.mk         # Payload build template (no module headers)
  include/           # Shared headers (types.h, ioscore.h, fs/, ios/, ...)
  lib/               # libcore.a (built from core/)
  modules/
    moduleTemplate.ld     # Linker script for standalone ELF modules
    embeddedModuleTemplate.ld  # Linker script for embedded modules
```

## Module Makefile Variables
```makefile
SOURCES    := source $(wildcard source/*/)   # dirs with .c/.cpp/.S
INCLUDES   := source                          # dirs for -iquote
DATA       :=                                  # binary data dirs
PROCESSID  := 0xNN    # hex process ID (affects ELF segment flags)
PRIORITY   := 0xNN    # thread priority
VIRTUALADDR := 0x2NNNNNNN  # virtual load address for .module section
PHYSADDR   := 0x1NNNNNNN   # physical address in kernel image
STACKSIZE  := 0xNNNN  # stack size in bytes

include $(SDKDIR)/modules.mk
```

## Module Build Flow (modules.mk)
1. Vars set in module Makefile
2. `modules.mk` auto-discovers all `.c/.cpp/.S/.s` files in SOURCES dirs
3. Adds `-D__PRIORITY=$(PRIORITY)` and `-Wl,-T$(TARGET).ld`
4. Links with `-lcore` (from `sdk/lib/libcore.a`)
5. Section start: `--section-start,.module=$(VIRTUALADDR)`
6. Outputs: `$(TARGET)-sym.elf` (with symbols), `$(TARGET).elf` (stripped)
7. NM run: `$(TARGET).lst` (symbol table)

## Adding a New Source File
- Drop `.c` or `.S` file into any directory already in `SOURCES`
- `SOURCES := source $(wildcard source/*/)` auto-discovers subdirectory `.c` files
- No manual `OFILES` update needed — it's fully automatic

## Linker Script (moduleTemplate.ld)
- Entry point: `_module_startup` (from `core/source/_module_startup.s`)
- Sections: `.module` (ro/text), `.module.data`, `.module.bss`
- Note segment: bakes `ProcessID`, `EntryPoint`, `Priority`, `StackSize`, `StackAddress` into ELF note
- Template vars: `__PROCESSID__`, `__VIRTADDR__`, `__PHYSADDR__`, `__PRIORITY__`, `__STACKSIZE__`
  replaced by the linker script generation step (generated as `$(TARGET).ld` in build dir)

## Module Address Map (known)
| Module | VIRTUALADDR | PHYSADDR | PROCESSID | PRIORITY |
|--------|------------|----------|-----------|---------|
| es | 0x20100000 | 0x139F0000 | 0x01 | 0x79 |
| fs | 0x20000000 | 0x13A10000 | 0x02 | 0x58 |

## Library Build (core/)
- `core/Makefile` builds `libcore.a` into `sdk/lib/`
- All modules link against `-lcore`
- Core provides: syscall wrappers, string utils, vsprintf, FS API

## Top-Level Makefile
- `make` at root builds kernel + all modules
- Embeds module ELFs into kernel image as binary data
- `es_module_bin.h`, `fs_module_bin.h` etc. generated during kernel build
