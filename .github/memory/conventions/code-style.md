# StarStruck Code Style

## .clang-format (/.clang-format)
- Language: Cpp
- IndentWidth: 4, ColumnLimit: 110
- BraceWrapping: Always (braces on new lines for functions, control flow, structs, classes, enums)
- AllowShortBlocksOnASingleLine: false, AllowShortIfStatementsOnASingleLine: Never
- BreakBeforeBraces: Custom (always wrap)
- AlignConsecutiveMacros: AcrossEmptyLinesAndComments
- IndentCaseLabels: true
- PointerAlignment: not derived, use explicit style

## Include Guards
- Newer files: `#pragma once`
- Older files: `#ifndef __FILENAME_H__` / `#define __FILENAME_H__` / `#endif`
- Mixed in the codebase — prefer `#pragma once` for new files

## Type System (core/include/types.h)
```c
typedef unsigned char      u8;   typedef signed char      s8;
typedef unsigned short     u16;  typedef signed short     s16;
typedef unsigned int       u32;  typedef signed int       s32;
typedef unsigned long long u64;  typedef signed long long s64;
typedef volatile u8/u16/u32/u64  vu8/vu16/vu32/vu64;
typedef volatile s8/s16/s32/s64  vs8/vs16/vs32/vs64;
typedef u32 size_t;
```
- `#include <stddef.h>` and `<stdbool.h>` are included via types.h

## Naming Conventions
| Kind | Convention | Examples |
|------|-----------|---------|
| Functions | PascalCase | `GetTitleUserId`, `HandleSyscall`, `OSCreateThread` |
| OS wrapper functions | `OS` prefix + PascalCase | `OSReadFD`, `OSCreateHeap` |
| Struct/union types | PascalCase | `IpcMessage`, `TitleUIDEntry`, `SFFSStatistics` |
| Struct fields | PascalCase | `FileDescriptor`, `MessageData`, `UserId` |
| Enum types | PascalCase | `TitleType`, `AccessMode`, `SeekMode` |
| Enum members | PascalCase | `SystemTitle`, `ReadWrite`, `SeekSet` |
| Macros | UPPER_SNAKE | `ALIGNED`, `STACK_ALIGN`, `MODULE_DATA`, `CHECK_SIZE` |
| IPC command macros | UPPER_SNAKE | `IOS_OPEN`, `IOS_CLOSE`, `IOS_IOCTL` |
| Local variables | camelCase | `messageQueueMessages`, `entryCount`, `ipcMessage` |
| Global constants/labels | UPPER_SNAKE or PascalCase | context-dependent |
| File names | lowercase with underscores or plain lower | `syscall_asm.s`, `fs.c`, `iosc.h` |

## Struct Pattern
```c
typedef struct
{
    u32 Field1;
    u16 Field2;
    u8  Field3;
} MyStruct;
CHECK_SIZE(MyStruct, 0xNN);
CHECK_OFFSET(MyStruct, 0xNN, Field1);
// etc.
```
- `__attribute__((packed))` used when structure has non-aligned fields
- Always add CHECK_SIZE + CHECK_OFFSET for protocol/hardware structs

## Comment Style
- File header: block comment with project name, file purpose, copyright + GPL-2.0
- Single-line: `//` (C++ style)
- Block: `/* ... */` (C style, used in file headers and older code)
- Inline alignment allowed for tables

## License Header Template
```c
/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	<module> - <short description>

	Copyright (C) <year>	<Author>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/
```

## Pragma Pattern (syscalls.h)
```c
#pragma GCC push_options
#pragma GCC optimize("O1")
// ... declarations ...
#pragma GCC pop_options
```
Used to prevent GCC from optimizing away syscall wrapper calls.
