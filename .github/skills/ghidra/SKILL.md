---
name: ghidra
description: Ghidra / MCP workflow notes for exporting decompilation results and verifying syscall numbers. Use when working with Ghidra and the MCP bridge.
license: MIT
---

# Ghidra / MCP — Skill Notes

This repository expects a Ghidra-based reverse-engineering workflow. The upstream RE assets are available in a Ghidra project (IOS58). Use Ghidra MCP for fast cross-referencing and to import decompilation results into repo memory.

Recommendations:
- Load the IOS58 kernel binary into Ghidra and confirm the program named `IOS58_Kernel` (image base `0x138f0000` as used in local RE notes).
- Use the MCP bridge (Ghidra MCP) to export function signatures, labels, and plate comments when updating repo memory.
- When deriving syscall numbers/addresses, cross-check the kernel `kernel/source/*` implementations and the Ghidra function names.

Practical tips:
- Keep Ghidra decompilation as a read-only authoritative source. Do not change semantics in implementation; prefer helper wrappers and named constants.
- Use the MCP tools to bulk-export function documentation into `docs/memory/reverse-engineering/` if needed.
