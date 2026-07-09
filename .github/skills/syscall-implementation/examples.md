# Examples & Reference

This file contains code snippets, examples and repository-relative build commands referenced from `SKILL.md`.

## Kernel implementation example

```c
s32 IOSC_MyFunction(u32 keyHandle, const void* inputBuf, u32 inputSize, void* outputBuf)
{
    s32 ret = IPC_SUCCESS, keyRet = IPC_SUCCESS;
    IOSC_BEGIN_SAFETY_WRAPPER(ret, keyRet);

    do {
        keyRet = IOSC_CheckCurrentProcessOwnsKey(keyHandle);
        if (keyRet != IPC_SUCCESS) break;

        ret = IOSC_CheckCurrentProcessCanRead(inputBuf, inputSize);
        if (ret != IPC_SUCCESS) break;

        ret = IOSC_CheckCurrentProcessCanReadWrite(outputBuf, inputSize);
        if (ret != IPC_SUCCESS) break;

        ret = _IOSC_MyFunctionInner(keyHandle, inputBuf, inputSize, outputBuf);
    } while (0);

    IOSC_END_SAFETY_WRAPPER(ret, keyRet);
    return ret;
}
```

## Assembly stub example

```asm
.macro _SYSCALL name, syscall
    .globl \name
    BEGIN_ASM_FUNC \name
        swi     \syscall
        bx      lr
    END_ASM_FUNC
.endm

_SYSCALL OSIOSCMyFunction,    0x00NN
```

## Quick rebuild commands (repository-relative)

From the repository root run:

```bash
make -C core
make -C modules/es    # or: make -C kernel
```

## File locations (repo-relative)

- `kernel/source/crypto/iosc.c`
- `kernel/source/crypto/iosc.h`
- `kernel/source/interrupt/syscall.cpp`
- `core/include/ios/syscalls.h`
- `core/source/ios/syscalls_asm.s`
