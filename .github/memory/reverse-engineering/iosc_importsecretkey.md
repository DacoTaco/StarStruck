# IOSC_ImportSecretKey reverse-engineering notes

## Behavior verified from Ghidra decompilation

- `IOSC_ImportSecretKey` begins by discarding a message from the IOSC message queue and switching stacks before primary logic.
- `importedHandle == 0xFFFFFFFF` is a special case: ownership is not checked for the target key slot.
- For signed secret import modes (`IOSC_SECRET_SIGNED`, `IOSC_SECRET_SIGNED_ENCRYPTED`):
  - `verifyHandle == 0xFFFFFFFF` bypasses the ownership check.
  - The function still reads the signature size from `verifyHandle` and validates `signBuffer`.
- For encrypted secret import modes (`IOSC_SECRET_ENCRYPTED`, `IOSC_SECRET_SIGNED_ENCRYPTED`):
  - `decryptHandle == 0xFFFFFFFF` bypasses the ownership check.
  - The IV buffer is still validated with `CheckMemoryPointer(ivData, 0x10, ...)`.
- Key buffer validation uses the padded AES size when encryption is enabled.

## Fix implemented

- Adjusted `kernel/source/crypto/iosc.c` so `IOSC_ImportSecretKey` matches the decompiled IOS semantics:
  - skip ownership checks for `importedHandle == (u32)-1`
  - skip ownership checks for `verifyHandle == (u32)-1` when signed
  - skip ownership checks for `decryptHandle == (u32)-1` when encrypted

## Notes

- This behavior is specific to the outer `IOSC_ImportSecretKey` wrapper, not the internal helper.
- The special `0xFFFFFFFF` handle is preserved in the outer wrapper for ownership bypass, but the internal helper rejects `importedHandle == 0xFFFFFFFF` as invalid.
