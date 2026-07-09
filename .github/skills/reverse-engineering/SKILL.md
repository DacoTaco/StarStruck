---
name: reverse-engineering
description: Guiding principles and workflow for reverse-engineering and porting behavior from Ghidra to repository code. Use when asked to analyze decompiled code or preserve behavior.
license: MIT
---

# Reverse-Engineering Skill

Guiding principles for RE and porting behavior to code:

- Preserve observable behavior and side-effects exactly (hardware registers, MMIO ordering, sync points).
- Prefer small, well-named helpers over wholesale rearchitecting of control flow discovered in decompilation.
- Validate assumptions by cross-referencing Ghidra function boundaries, symbol names, and kernel syscall tables.

Workflow:
1. Use Ghidra MCP to locate the relevant function(s) and confirm prototypes.
2. Extract types and offsets; add or update `docs/memory/reverse-engineering/` notes and `docs/skills/` skill files.
3. Implement minimal, behavior-preserving C code in the repo; add tests or sanity checks where feasible.
4. Iterate: keep both the repository code and the Ghidra comments in sync.
