# Implementation Gotchas

## IPC Module Main Loop Pattern
Every IOS module follows this exact structure:

```c
int main(void)
{
    u32 messageQueueMessages[8] ALIGNED(0x20) = { 0 };
    // Set priority twice (IOS quirk — first sets scheduler, second actual)
    OSSetThreadPriority(0, 0x50);
    OSSetThreadPriority(0, 0x79);   // final priority
    printk("$IOSVersion:  <NAME>: %s %s 64M $", __DATE__, __TIME__);

    s32 ret = OSCreateMessageQueue((void**)&messageQueueMessages, 8);
    s32 queueId = ret;
    if (ret >= 0)
        ret = OSRegisterResourceManager("/dev/<name>", queueId);
    if (ret < IPC_SUCCESS)
        return ret;

    // init hardware/state...

    while (1)
    {
        IpcMessage* msg;
        OSReceiveMessage(queueId, (u32**)&msg, 0);
        s32 result;
        switch (msg->Request.Command)
        {
            case IOS_OPEN:   result = HandleOpen(msg);  break;
            case IOS_CLOSE:  result = HandleClose(msg); break;
            case IOS_IOCTL:  result = HandleIoctl(msg); break;
            case IOS_IOCTLV: result = HandleIoctlv(msg); break;
            default:         result = IPC_EINVAL;       break;
        }
        OSResourceReply(msg, result);
    }
}
```

## Message Queue Alignment
- `messageQueueMessages[]` array MUST be `ALIGNED(0x20)` (32-byte align)
- Reason: DMA cache coherency — IOS/Hollywood requires 32-byte aligned IPC buffers
- ES uses `ALIGNED(0x20)`, fs uses `ALIGNED(0x10)` — use 0x20 to be safe

## IPC Message Types
```c
// IpcRequest.Command values (core/include/ios/ipc.h):
IOS_OPEN=0x01, IOS_CLOSE=0x02, IOS_READ=0x03, IOS_WRITE=0x04,
IOS_SEEK=0x05, IOS_IOCTL=0x06, IOS_IOCTLV=0x07
// Accessing fields:
msg->Request.Message.Open.Filepath   // IOS_OPEN
msg->Request.Message.Ioctl.InputBuffer / .InputLength / .IoBuffer / .IoLength
msg->Request.Message.Ioctlv.Ioctl / .InputArgc / .IoArgc / .MessageData
msg->Request.FileDescriptor          // for non-OPEN commands
msg->Request.Message.Open.UID / .GID // caller's process UID/GID
```

## FS Path Limits
- `MAX_FILE_PATH = 0x40` (64 chars including null terminator)
- `MAX_PATH_DEPTH = 8` (directory nesting limit)
- All NAND paths are absolute, start with `/`
- FS device names: `/dev/fs`, `/dev/boot2`, `/dev/flash`
- ES owns: `/title/`, `/ticket/`, `/sys/uid.sys`, `/sys/cert.sys`

## FS Error Codes vs IPC Error Codes
- `IPC_SUCCESS = 0` — general success
- `IPC_ENOENT = -6`, `IPC_ENOMEM = -22`, `IPC_EINVAL = -4`, `IPC_EACCES = -1`
- `FS_ENOENT`, `FS_EINVAL` etc. (different values! from `<fs/errors.h>`)
- `ES_EINVAL = -1017`, `ES_ENOMEM = -1024`, `ES_EACCES = -1026`, `ES_NO_TICKET = -1028`
- `IOSC_EACCES = -2000`, `IOSC_EINVAL = -2002`
- Check the right error namespace for each subsystem

## Memory Allocation
- Heap ID 0 = kernel/global heap (`KERNEL_HEAPID 0` in es/filesystem/fs.c)
- `OSAllocateMemory(heapid, size)` — no alignment guarantee
- `OSAlignedAllocateMemory(heapid, size, align)` — for aligned buffers
- `OSFreeMemory(heapid, ptr)` — must match heapid used for alloc
- Always check for NULL return from alloc, return `IPC_ENOMEM` or `ES_ENOMEM`

## Cleanup / goto Pattern
ES and FS code use `goto` for cleanup (C99 style, not exceptions):
```c
s32 SomeFunction(...)
{
    void* buf = NULL;
    s32 fd = -1;
    s32 ret = 0;

    fd = OSOpenFD(...);
    if (fd < 0) { ret = fd; goto cleanup; }

    buf = OSAllocateMemory(KERNEL_HEAPID, size);
    if (!buf) { ret = IPC_ENOMEM; goto cleanup; }

    // ... work ...

cleanup:
    if (fd >= 0) OSCloseFD(fd);
    if (buf) OSFreeMemory(KERNEL_HEAPID, buf);
    return ret;
}
```

## MODULE_DATA / MODULE_BSS Attributes
```c
// Symbols that must be in the module's own data/bss segments:
MODULE_DATA const u8 myArray[] = { ... };  // section(".module.data")
MODULE_BSS static u32 myState;             // section(".module.bss")
// Required for data that the kernel maps per-module. Global C data without
// this attribute may end up in wrong segment.
```

## Module Startup
- Entry point is `_module_startup` (core/source/_module_startup.s), NOT `main`
- `_module_startup` sets up stack pointer, calls `OSSetThreadPriority(0, priority)`,
  then calls `main()`
- `__stackEnd` and `__priority` are linker symbols from the LD script

## Thumb Code in ARM Context
- Modules compile as Thumb (`-mthumb`)
- Syscall wrappers are ARM (`.arm` directive in syscalls_asm.s)
- Interwork handled by ARM/Thumb interop — `bx lr` in wrappers
- Don't mix ARM/Thumb in same .c file without `__attribute__((target("arm")))`

## IOSC Keyslot Handle Semantics
- IOSC handles are `u32` — NOT pointers
- Created with `OSIOSCCreateObject(u32* handle_out, type, subtype)`
- Must be deleted with `OSIOSCDeleteObject(handle)` after use
- Ownership check: use `OSIOSCGetOwnership` — returns process ID mask
- `CheckKeyslotPermissions` (ES crypto): verify caller uid owns the slot
- Common key handles in ES stored as global array (2 entries: normal + Korean)

## FS Open Pattern (used by ES filesystem/)
```c
// From core/include/fs/fs.h — thin ioctlv wrappers to /dev/fs:
s32 fd = OpenFile("/path/to/file", Read);  // NOT OSOpenFD directly
// Errors: FS_ENOENT (-1 mapped), FS_EACCES, etc. — different from IPC codes
// After use:
OSCloseFD(fd);  // Use OSCloseFD (not a FS wrapper — fd is a kernel fd)
```

## Struct Field Access in IoctlvMessageData
```c
// For IOS_IOCTLV:
IoctlvMessageData* vectors = msg->Request.Message.Ioctlv.MessageData;
u32 inputCount = msg->Request.Message.Ioctlv.InputArgc;
u32 ioCount    = msg->Request.Message.Ioctlv.IoArgc;
// Input vectors: vectors[0..inputCount-1]
// IO vectors:    vectors[inputCount..inputCount+ioCount-1]
void* inBuf = vectors[0].Data;
u32   inLen = vectors[0].Length;
```

## Include Paths in Modules
```c
#include <types.h>           // sdk/include/types.h (via -I sdk/include)
#include <ios/syscalls.h>    // sdk/include/ios/syscalls.h
#include <fs/fs.h>           // sdk/include/fs/fs.h
#include <fs/errors.h>       // sdk/include/fs/errors.h
#include "types.h"           // module-local (source/types.h) via -iquote source
#include "filesystem/fs.h"   // module-local subdirectory
```
- `<>` includes use SDK include path
- `""` includes use `-iquote source` (module's own source dir)
- Module Makefile sets `INCLUDES := source` → `-iquote $(CURDIR)/source`

## IOS Version String Pattern
All modules print at startup:
```c
printk("$IOSVersion:  <MODULE>: %s %s 64M $", __DATE__, __TIME__);
```
The `$IOSVersion:` prefix is used by IOS for identification.

## Compound Literal Initializer (C99)
Used in ES code for struct init:
```c
entries[i].Title = (TitleID){ .Type = titleType, .Id = titleId };
```
Valid in C99 (all modules are C), not just C++.
