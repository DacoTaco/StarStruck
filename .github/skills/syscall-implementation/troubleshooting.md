# Troubleshooting

Common failures and checks when wiring a new syscall.

1. Build/link errors
   - Missing OS wrapper declaration in `core/include/ios/syscalls.h` or placed after `#pragma GCC pop_options`.
   - Assembly stub missing or using wrong syscall number causing symbol mismatch.

2. Runtime failures
   - Ownership checks failing: ensure the caller has the expected keyslot/permissions.
   - Memory pointer checks failing: use the correct `IOSC_CheckCurrentProcessCanRead*` helpers.

3. Syscall index mismatches
   - Verify the numeric slot used for `_SYSCALL` matches the `SYSCALL(...)` table index and Ghidra RE notes.

4. Debugging tips
   - Rebuild `core` and the module, then inspect `modules/<module>/build/<module>.lst` for unresolved symbols.
   - Cross-check the syscall number / function address in Ghidra and ensure the table index aligns.
