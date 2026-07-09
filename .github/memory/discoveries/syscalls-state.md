# Syscalls State

## How Syscalls Work
- Kernel: SVC/SWI instruction (StarStruck), IOS used undefined instruction trap
- Syscall number is the SWI immediate operand
- Handler table: `syscall_handlers[]` in `kernel/source/interrupt/syscall.cpp`
- Entry format: `{ .Handler = ptr, .StackArgumentCount = N }`
  - StackArgumentCount = args > 4 ? args - 4 : 0 (for register overflow to stack)
- `SYSCALL(func)` macro: auto-deduces stack arg count via template
- `SYSCALL_NULL` = `{ nullptr, 0 }` (unimplemented)

## OS Wrapper Pattern (core)
Two files must be updated together:

1. **`core/include/ios/syscalls.h`** — declare C prototype:
   ```c
   s32 OSMyNewSyscall(type1 arg1, type2 arg2);
   ```

2. **`core/source/ios/syscalls_asm.s`** — emit SWI stub:
   ```asm
   _SYSCALL OSMyNewSyscall, 0x00NN
   ```

### Exact `_SYSCALL` macro (from syscalls_asm.s):
```asm
.macro _SYSCALL name, syscall
    .globl \name
    BEGIN_ASM_FUNC \name
        swi     \syscall
#       .long (0xE6000010 | (\syscall << 5))  /* IOS-style undefined-instr alternative */
        bx      lr
    END_ASM_FUNC
.endm
```
- All stubs are `.arm` mode (file starts with `.arm` directive)
- File is wrapped in `#pragma GCC push_options` / `#pragma GCC optimize("O1")`
- Entries appear in ascending numeric order; entries DO NOT have to be contiguous
  (see gaps at 0x35–0x3E, 0x44–0x46, etc. in the file)

## Adding a New Syscall (full checklist)
1. Implement the kernel function in `kernel/source/` (e.g., `kernel/source/crypto/iosc.c`)
2. Declare it in the appropriate kernel `.h` (e.g., `kernel/source/crypto/iosc.h`)
3. Add `SYSCALL(KernelFunctionName)` at the correct index in `syscall_handlers[]` in
   `kernel/source/interrupt/syscall.cpp` (replace `SYSCALL_NULL` at that index)
4. If the header isn't already `#include`d at the top of `syscall.cpp`, add it
   (IOSC functions: `iosc.h` is already included; check existing includes first)
5. Add OS wrapper declaration to `core/include/ios/syscalls.h`
   — insert before `#pragma GCC pop_options` at end of file
   — wrap the whole file in `#pragma GCC push_options` / `#pragma GCC pop_options` with `optimize("O1")` (already present)
6. Add `_SYSCALL OSFunctionName, 0xNNNN` to `core/source/ios/syscalls_asm.s`
   — insert in ascending numeric order; gaps are allowed (do not need to be contiguous)
7. Rebuild `core/` (regenerates `sdk/lib/libcore.a`) then rebuild the module:
   ```
   make -C /media/windows/Projects/0-Private/StarStruck/core
   make -C /media/windows/Projects/0-Private/StarStruck/modules/es
   ```

## ES-Needed Syscalls Status

### Non-IOSC
| # | IOS Name | Kernel Function | Table Entry | OS Wrapper |
|---|---------|----------------|-------------|-----------|
| 0x41 | IOS_StartPPC (plan) | `LoadBinary` | ✅ `SYSCALL(LoadBinary)` | ✅ `OSLoadBinary` |
| 0x42 | IOS_ios_boot (plan) | `LoadKernel` | ✅ `SYSCALL(LoadKernel)` | ✅ `OSLoadKernel` |
| 0x43 | — | `LaunchKernel` | ✅ `SYSCALL(LaunchKernel)` | ✅ `OSLaunchKernel` |
| 0x47 | IOS_get_kernel_flavor | `GetKernelFlavor` | ✅ `SYSCALL(GetKernelFlavor)` | ✅ `OSGetKernelFlavor` |
| 0x4D | IOS_GetLoMemOSVersion | `GetIosVersion` | ✅ `SYSCALL(GetIosVersion)` | ✅ `OSGetIosVersion` |
| 0x54 | SetPPCACRPerms | `SetPPCACRPerms` | ❌ `SYSCALL_NULL` | ❌ missing |
| 0x59 | SetIpcAccessRights | `SetIpcAccessRights` | ❌ `SYSCALL_NULL` | ❌ missing |
| 0x5A | — | `LaunchModule` | ✅ `SYSCALL(LaunchModule)` | ✅ `OSLaunchModule` |

### IOSC Syscalls
| # | IOS Name | Status | Notes |
|---|---------|--------|-------|
| 0x5B | CreateObject | ✅ `SYSCALL(IOSC_CreateObject)` | ✅ wrapper `OSIOSCCreateObject` |
| 0x5C | DeleteObject | ✅ | ✅ `OSIOSCDeleteObject` |
| 0x5D | ImportSecretKey | ✅ `SYSCALL(IOSC_ImportSecretKey)` | ✅ OSIOSCImportSecretKey |
| 0x5E | (unknown/unused) | `SYSCALL_NULL` | No known IOS function at this slot |
| 0x5F | ImportPublicKey | ✅ `SYSCALL(IOSC_ImportPublicKey)` | ✅ `OSIOSCImportPublicKey` |
| 0x60 | (unknown/unused) | `SYSCALL_NULL` | No known IOS function at this slot |
| 0x61 | ComputeSharedKey | ❌ `SYSCALL_NULL` | ❌ no wrapper |
| 0x62 | SetData | ✅ | ✅ `OSSetIOSCData` |
| 0x63 | GetData | ✅ | ✅ `OSGetIOSCData` |
| 0x64 | GetKeySize | ✅ | ✅ `OSIOSCGetKeySize` |
| 0x65 | GetSignatureSize | ✅ | ✅ `OSIOSCGetSignatureSize` |
| 0x66 | GenerateHashAsync | ✅ | ✅ `OSIOSCGenerateHashAsync` |
| 0x67 | GenerateHash | ✅ `SYSCALL(IOSC_GenerateHash)` | ✅ `OSIOSCGenerateHash` |
| 0x68 | EncryptAsync | ✅ | ✅ `OSIOSCEncryptAsync` |
| 0x69 | Encrypt | ✅ | ✅ `OSIOSCEncrypt` |
| 0x6A | DecryptAsync | ✅ | ✅ `OSIOSCDecryptAsync` |
| 0x6B | Decrypt | ✅ | ✅ `OSIOSCDecrypt` |
| 0x6C | VerifyPublicKeySign | ❌ `SYSCALL_NULL` | ❌ no wrapper |
| 0x6D | GenerateBlockMAC | ✅ | ✅ `OSIOSCGenerateBlockMAC` |
| 0x6E | GenerateBlockMACAsync | ✅ | ✅ `OSIOSCGenerateBlockMACAsync` |
| 0x6F | ImportCertificate | ❌ `SYSCALL_NULL` | ❌ no wrapper |
| 0x70 | GetDeviceCertificate | ❌ `SYSCALL_NULL` | ❌ no wrapper |
| 0x71 | SetOwnership | ❌ `SYSCALL_NULL` | ❌ no wrapper |
| 0x72 | GetOwnership | ❌ `SYSCALL_NULL` | ❌ no wrapper |
| 0x73 | (unknown/unused) | `SYSCALL_NULL` | No known IOS function at this slot |
| 0x74 | GenerateKey | ❌ `SYSCALL_NULL` | ❌ no wrapper |
| 0x75 | GeneratePublicKeySign | ❌ `SYSCALL_NULL` | ❌ no wrapper |
| 0x76 | GenerateCertificate | ❌ `SYSCALL_NULL` | ❌ no wrapper |

## OS Wrapper Signatures for Missing Syscalls

### Non-IOSC
```c
// 0x4D — reads IOS version from LoMem 0x80003140; only callable by ES
u32 OSGetIosVersion(void);   // returns 32-bit IOS version

// 0x54 — set PPC ACR permission bits; enable=1 grants, enable=0 revokes
void OSSetPPCACRPerms(u8 enable);

// 0x59 — set per-process IPC access rights bitmask; only callable by ES
s32 OSSetIpcAccessRights(const u8* rights);
```

### IOSC
```c
// 0x5D
s32 OSIOSCImportSecretKey(u32 importedHandle, u32 verifyHandle, u32 decryptHandle,
                          u32 flag, const u8* sig, const u8* iv, const u8* key);
// 0x61
s32 OSIOSCComputeSharedKey(u32 privKeyHandle, u32 pubKeyHandle, u32 sharedKeyHandle);
// 0x6C
s32 OSIOSCVerifyPublicKeySign(const void* data, u32 size, u32 keyHandle, const void* sig);
// 0x6F
s32 OSIOSCImportCertificate(const void* cert, u32 signerHandle, u32 pubKeyHandle);
// 0x70  — ECCCert is 0x180 bytes
s32 OSIOSCGetDeviceCertificate(void* certOut);   // certOut = ECCCert* (0x180 bytes)
// 0x71
s32 OSIOSCSetOwnership(u32 keyHandle, u32 pidMask);
// 0x72
s32 OSIOSCGetOwnership(u32 keyHandle, u32* ownershipOut);
// 0x74
s32 OSIOSCGenerateKey(u32 keyHandle);
// 0x75  — sig output is 0x3c (60) bytes ECC signature
s32 OSIOSCGeneratePublicKeySign(const void* hash, u32 hashLen, u32 keyHandle, void* sigOut);
// 0x76  — name is the device certificate name string
s32 OSIOSCGenerateCertificate(u32 keyHandle, const char* name, void* certOut);
```

## Syscall Names: Plan vs Code
The ES reimplementation plan uses IOS 58 syscall names. StarStruck has renamed several:
- IOS 0x41 `IOS_StartPPC` → StarStruck `LoadBinary` (same slot)
- IOS 0x42 `IOS_ios_boot` → StarStruck `LoadKernel` (same slot)
Both are implemented. The plan's Step 1a notes about 0x41/0x42 being SYSCALL_NULL is outdated for these.

## Missing Non-IOSC Syscall Implementation Guide

### 0x4D — GetLoMemOSVersion
- IOS ref: `ffff6150` — reads `_Mem1_IOSVersion` (LoMem at `0x80003140`)
- Only callable by ES process; returns NULL if not ES
 - Only callable by ES process; returns 0 if not ES
 - StarStruck impl: read a single `u32` from `MEM1_IOSVERSION` and return it
 - OS wrapper: `u32 OSGetIosVersion(void)` → returns the 32-bit IOS version

### 0x54 — SetPPCACRPerms
- IOS ref: `ES_TriggerActiveTitleAccessRights` calls it to toggle PPC ACR access
- Sets a hardware register (ACR permissions) based on `enable` flag
- Used when: `tmd->AccessRights` bit 31 set → enable=1, else enable=0
- StarStruck impl: write to `HW_ACR` register (find exact register from existing HW headers)
- OS wrapper: `void OSSetPPCACRPerms(u8 enable)`

### 0x59 — SetIpcAccessRights
- IOS ref: `ffff2ed4` — sets the IPC hash table for resource manager access control
- Only callable by ES process (enforced in kernel)
- Takes `const u8* rights` — a bitmask buffer describing per-RM access
- StarStruck impl: call into kernel resource manager subsystem to update access control
- OS wrapper: `s32 OSSetIpcAccessRights(const u8* rights)`

---

## IOSC Kernel Syscall Implementation Pattern

Most IOSC functions in the kernel follow this pattern using the macros defined in `iosc.c`:

```c
// IMPORTANT: Use these macros exactly as shown — do NOT write the goto/swap manually.
// Macro definitions (already in iosc.c — do not redeclare):
//   IOSC_BEGIN_SAFETY_WRAPPER(mainRet, keyRet)
//   IOSC_END_SAFETY_WRAPPER(mainRet, keyRet)
//
// The macros expand to:
//   keyRet = IOSC_DiscardMessageFromQueue(IOSC_Information.messageQueueId);
//   mainRet = IOSC_EACCES;         // <-- default on failure to acquire mutex
//   if (keyRet == IPC_SUCCESS) {
//       IOSC_SwapStack(CurrentThread->DefaultThreadStack, IOSC_SafeStackEnd);
//       <body>
//   }
//   IOSC_SwapStack(IOSC_SafeStackEnd, CurrentThread->DefaultThreadStack);
//   keyRet = IOSC_SendEmptyMessageToQueue(IOSC_Information.messageQueueId);
//   if (keyRet != IPC_SUCCESS && mainRet == IPC_SUCCESS) mainRet = IOSC_EACCES;

s32 IOSC_FunctionName(args...)
{
    s32 ret = IPC_SUCCESS, keyRet = IPC_SUCCESS;
    IOSC_BEGIN_SAFETY_WRAPPER(ret, keyRet)

    do
    {
        // 1. Check caller owns the keyhandle(s)
        keyRet = IOSC_CheckCurrentProcessOwnsKey(keyHandle);
        if (keyRet != IPC_SUCCESS)
            break;

        // 2. Validate memory pointer access
        ret = IOSC_CheckCurrentProcessCanRead(buf, size);       // read-only args
        if (ret != IPC_SUCCESS)
            break;
        ret = IOSC_CheckCurrentProcessCanReadWrite(outBuf, size); // output args
        if (ret != IPC_SUCCESS)
            break;

        // 3. Do actual work
        ret = _IOSC_FunctionNameInner(args...);
    }
    while (0);

    IOSC_END_SAFETY_WRAPPER(ret, keyRet)
    return ret;
}
```

Key details from actual code:
- Field: `CurrentThread->DefaultThreadStack` (NOT `default_stack_top`)
- Default error on mutex failure: `IOSC_EACCES` (NOT `IOSC_EUNKNOWN`)
- Body is `do { ... } while(0)` with `break` — NOT goto/labels
- Both `ret` and `keyRet` must be declared as `s32` at function top
- `IOSC_CheckCurrentProcessOwnsKey` result goes in `keyRet` (controls macro flow)
- Memory checks go in `ret` (the function return value)
- Simpler functions (no key ownership check, like `IOSC_GenerateHash`) skip the macro
  and do direct `CheckMemoryPointer` calls + return the inner call directly

Key helpers already in `iosc.c` (all static inline or static):
- `IOSC_DiscardMessageFromQueue` / `IOSC_SendEmptyMessageToQueue`
- `IOSC_SwapStack` (extern from `iosc_helpers.s`)
- `IOSC_CheckCurrentProcessOwnsKey(u32 handle)` — checks PID mask in keyring
- `IOSC_CheckCurrentProcessCanRead(const void* ptr, u32 size)` — calls `CheckMemoryPointer(..., 3, ...)`
- `IOSC_CheckCurrentProcessCanReadWrite(const void* ptr, u32 size)` — calls `CheckMemoryPointer(..., 4, ...)`

### Missing IOSC Functions to Add to iosc.c / iosc.h

The inner helpers are `static` functions prefixed `_IOSC_` (e.g., `_IOSC_Decrypt`, `_IOSC_Encrypt`).
All inner helpers are static to iosc.c — only the outer wrappers (no underscore prefix) are exported
and declared in iosc.h.

| Syscall | Outer function (exported) | Inner static helper | Kernel file | Signature |
|---------|--------------------------|--------------------|--------------|-----------| 
| 0x5D | `IOSC_ImportSecretKey` | `_IOSC_ImportSecretKeyInner` | iosc.c | `(u32 importedHandle, u32 verifyHandle, u32 decryptHandle, u32 flag, const u8* sig, const u8* iv, const u8* key)` |
| 0x61 | `IOSC_ComputeSharedKey` | `_IOSC_ComputeSharedKeyInner` | iosc.c | `(u32 privKeyHandle, u32 pubKeyHandle, u32 sharedKeyHandle)` |
| 0x6C | `IOSC_VerifyPublicKeySign` | `_IOSC_VerifyPublicKeySignInner` | iosc.c | `(const void* data, u32 size, u32 keyHandle, const void* sig)` |
| 0x6F | `IOSC_ImportCertificate` | `_IOSC_ImportCertificateInner` | iosc.c | `(const void* cert, u32 signerHandle, u32 pubKeyHandle)` |
| 0x70 | `IOSC_GetDeviceCertificate` | `_IOSC_GetDeviceCertificateInner` | iosc.c | `(void* certOut)` — certOut is 0x180 bytes R/W |
| 0x71 | `IOSC_SetOwnership` | (calls Keyring directly) | iosc.c | `(u32 keyHandle, u32 pidMask)` — OR's current PID into mask |
| 0x72 | `IOSC_GetOwnership` | (calls Keyring directly) | iosc.c | `(u32 keyHandle, u32* ownershipOut)` |
| 0x74 | `IOSC_GenerateKey` | `_IOSC_GenerateKeyInner` | iosc.c | `(u32 keyHandle)` |
| 0x75 | `IOSC_GeneratePublicKeySign` | `_IOSC_GeneratePublicKeySignInner` | iosc.c | `(const void* hash, u32 hashLen, u32 keyHandle, void* sigOut)` — sigOut is 0x3c bytes |
| 0x76 | `IOSC_GenerateCertificate` | `_IOSC_GenerateCertificateInner` | iosc.c | `(u32 keyHandle, const char* name, void* certOut)` |

Notes:
- `IOSC_SetOwnership` (0x71): ORs `(1 << CurrentThread->ProcessId)` into existing owner mask
- `IOSC_GetOwnership` (0x72): simpler — can call `Keyring_GetKeyOwnerProcess` directly, no inner needed
- `IOSC_GetDeviceCertificate` (0x70): output buffer is 0x180 (384) bytes; use `IOSC_CheckCurrentProcessCanReadWrite`
- `IOSC_GeneratePublicKeySign` (0x75): signature output is 0x3c (60) bytes (ECC signature)

## IOSC Keyring Constants (core/include/ios/keyring.h)
```c
KEYRING_CONST_NG_PRIVATE_KEY   // Device ECC private key
KEYRING_CONST_NG_ID            // Device ID
KEYRING_CONST_NAND_KEY         // NAND AES key
KEYRING_CONST_NAND_HMAC        // NAND HMAC key
KEYRING_CONST_OTP_COMMON_KEY   // Common AES key (from OTP)
KEYRING_CONST_OTP_RNG_SEED
KEYRING_CONST_SD_PRIVATE_KEY
KEYRING_CONST_EEPROM_COMMON_KEY
KEYRING_CUSTOM_START_INDEX     // Dynamic slots start here
```
- ES CommonKey handles stored at `PTR_ES_CommonKeyHandles_20103494` (2 handles: normal + Korean)
