# /dev/es — ES Module Reimplementation Plan

> **Reference**: IOS58 kernel dump (`00000017.app`), Ghidra program `IOS58`.
> ES module image: `0x20100000`–`0x2010c11f` (code), `0x2010d000`–`0x2010d17f` (data),
> `0x2010e000`–`0x201126ab` (bss). IOSC/crypto code lives in the kernel at
> `0x13a70000`–`0x13a7b487`; kernel syscalls at `0xffff0000+`.
> **Scope**: a complete, behaviour-matching reimplementation of `/dev/es`, ending with
> ES owning the boot flow exactly as IOS58 does.

Written for StarStruck contributors working from the Ghidra RE. Addresses are IOS58; names keep
the `ES_` prefix as they appear in Ghidra (the prefix is dropped in the C source).

---

## Project decisions (locked)

These were decided with the maintainer and drive the whole plan:

1. **Boot flow** — the plan **ends by handing the boot flow to ES** and removing the
   temporary boot2/HBC path in [kernel/source/main.c](../../kernel/source/main.c). That is the
   final step.
2. **Modules** — ES launches **Nintendo's modules from NAND**: it reads
   `/title/00000001/<IOS>/content/title.tmd` and `LaunchRM`s every content except the boot
   index, exactly as IOS58 does.
3. **Verification** — every step is validated **on real hardware and on an emulator**
   (Dolphin LLE), by exercising the ioctl(v) from a homebrew test client and comparing results
   against real IOS58, plus a real-use smoke test (boot System Menu, install/launch a title).
4. **Bugs** — **bug-for-bug**. The port reproduces IOS58's observable behaviour exactly,
   including the out-of-bounds accesses, fd/heap/key leaks and quirks listed in
   [Appendix C](#appendix-c--ios58-bugs-to-reproduce-bug-for-bug). Each one is called out at
   its step so it is deliberate, not an accident, and carries a comment in the source.

---

## RE status summary

The ES module is **almost fully reverse-engineered**. All ~130 ES functions decompile and the control flow is understood; a
per-function review of the decompile and disassembly found **no blocker**. What is left is listed as **RE to do** under the
step where it becomes relevant, and summarised here:

| Area | RE still to do | Needed at |
|------|----------------|-----------|
| ECC | `FieldInvert` and `EcScalarMultiplyPointInPlace` are inferred from the decompile; the E2/E4 tests confirm them. Unreferenced code at `0x13a7a388`–`0x13a7a3ff`; purpose of `ECDHParameters.Scalar`. | E2, E4 |
| IOSC | No RSA/ECC certificate struct exists in Ghidra (offsets are known, see 0x6f). Ghidra signatures of ECDSA sign/verify and the RSA wrappers drop their stack args. Meaning of the registers `0x0d8b4200/2` written by syscall `0x54`. | 0b |
| FS module | Rename onto an existing empty dir, recursive delete, ReadDir on a file entry, ioctl on a negative fd, ReadDir ordering. | 1b, 5, 7 |
| DI module | `/dev/di` ioctl `0x8e` (no DI module in the Ghidra DB). | 9 |
| Kernel | Confirm `LoadBinary`/`LoadKernel` match `StartPPC` (0x41) / `ios_boot` (0x42) semantics. | 9, 11 |
| ES | Whether ES boot always sets the active-title TMD before the PPC can call `0x24`/`0x3c`/`ChooseExportKey` (they dereference it unchecked). Meaning of TitleType bits `0x04`/`0x20` (behaviour is known). | 6, 7 |
| Ghidra DB | Struct/signature/name corrections (not blocking): see Phase 1a, Appendix D. | 1a |

The work sits in two places:

- **Kernel crypto is missing.** ES leans on IOSC syscalls that StarStruck does not implement,
  and those need ECC (sect233r1), RSA (2048/4096 PKCS#1 v1.5), ECDSA and a PRNG that the kernel
  does not have. This is the long pole. See [Phase 0](#phase-0--kernel-prerequisites).
- **Ghidra decompiler artifacts.** Six-plus functions carry "Heritage AFTER dead removal"
  warnings and show wrong call arguments (usually a dropped path string, or a hidden stack
  argument). These must be ported from the **disassembly**, not the decompile. Each is flagged
  at its step and in [Appendix D](#appendix-d--decompiler-artifacts-port-from-disassembly).

### Not needed
- **Async IOSC** (`0x66`/`0x68`/`0x6a`): ES has no veneer for them, so it never calls them.
  Its content crypto is **synchronous** `Encrypt`/`Decrypt` (`0x69`/`0x6b`).
- **`IOSC_CheckDiHashes` (`0x77`)**: ES has a stub but no veneer and no xref. ES never calls it;
  it is the DI module's. `DiVerifyInner` just copies `contents[i].Sha1Hash` into the caller's `hashes[]` array.

---

## Module facts (IOS58 reference)

- **Entry**: `ES_entrypoint` 0x20100000 sets SP from `__stackEnd` + size, priority `0x79`,
  then calls `ES_main`. Matches StarStruck's `_module_startup`.
- **Metadata** (set in [modules/es/Makefile](Makefile)): virtual `0x20100000`,
  stack `0x4000`, priority `0x79`, PID `ESId = 1`.
- **Startup (`ES_main` 0x201015e8)**: `SetThreadPriority(0,0x50)` then `(0,0x79)`;
  `IsNonDiscAuthenticated = 0`; create a **1-slot** message queue; `RegisterResourceManager("/dev/es")`;
  `CreateTimer(0,0,q,0xdeaddead)` (unarmed); `SetUid(self,0)`, `SetGid(self,0)`;
  `CreateStartupDirectories`; then the boot branch (see [Phase 11](#phase-11--boot-flow-handover-final)).
- **Dispatch (`ES_main` loop)**: on `0xdeaddead` → playtime-limit tick; else switch on command:
  `IOSOpen`→`HandleOpen`, `IOSClose`→`HandleClose`, `IOSIoctlv`→`HandleIoctlv`, else `-4`;
  `ResourceReply`. **ES implements only Open/Close/Ioctlv** — no Ioctl, Read, Write, Seek.
- **Context pool**: `ES_ContextArray` = `ESContext[3]` at `0x2010e000` (bss).
  `HandleOpen` matches path `"/dev/es"`, takes a free slot (`active==0`), memsets `0x1c0`,
  sets `ImportContext.currentContentFd/Index = -1`, `active=1`, copies `uid`/`gid` from the
  open message, and returns the **element pointer** as the IPC handle. No free slot → `-1016`;
  bad path → `-6`. `HandleClose` only clears `active` (leaking the TMD buffer and key object —
  reproduce, see Appendix C).

---

## Phase 0 — Kernel prerequisites

**Nothing in ES can be tested until the kernel side is done. This phase lands on `main` first.**
It has two parts: add the crypto primitives, then wire the new syscalls.

### 0a — Crypto primitives (the long pole)

StarStruck has AES, SHA-1 and the keyring, but **no public-key crypto**. All of it is new kernel
code in `kernel/source/crypto/`, split into the same source files IOS had (see [source layout](#source-layout-follows-ios)).

| Primitive | Needed by | IOS ref | Notes |
|-----------|-----------|---------|-------|
| **ECC sect233r1 + ECDH** | `0x61` now; field/point layer also reused by `0x70,0x74,0x75,0x76` | `0x13a700e8`–`0x13a7ad7f` | Split into steps E1–E6 in [0a.1](#0a1--ecc--ecdh). |
| **ECDSA** sign/verify | `0x75`/`0x6c`,`0x76` | `0x13a753a8`, `0x13a704dc` | `e = SHA-1` as 160-bit int; sig = `r‖s` (30+30 BE). Scalar = 30 PRNG bytes masked to 233 bits. n = `0x01000000000000000000000000000013E974E72F8A6922031D2603CFE0D7`, cofactor 2. Sign: `r = ((k mod n)·G).x mod n`, `s = k⁻¹(e + d·r) mod n`; an unreduced `k` can exceed n; a zero r or s returns 2, callers ignore it. Verify: `0<r,s<n`, `u1 = e/s`, `u2 = r/s`, `P = u1·G + u2·Q`, `P.x mod n == r`; `Q` is not validated. The mod-n layer (`0x13a772d0`–`0x13a77c60`) mirrors RSAREF `nn.c` and uses static scratch buffers (not reentrant); any correct bignum is acceptable. **RE to do:** Ghidra signatures drop the stack args: verify is `(hash, 0x14, params, Q, sig*, &result)`, sign is `(hash, 0x14, params, d, outRS, k)`. The verify wrapper `FUN_13a75428` loads both `Q` and `r‖s` with `ECC_LoadUInt233Pair` (`0x13a71fa8`, shared with ECDH); verify reads `sig` as two numbers `UInt233[2]`, never as a point. |
| **RSA** PKCS#1 v1.5 verify (2048 + 4096) | `0x6c`,`0x6f` | `FUN_13a75514`, `FUN_13a77a40` (modexp) | Block is `00 01 FF…FF 00` + DigestInfo (`30 21 30 09 06 05 2B 0E 03 02 1A 05 00 04 14`, at `0x13a75744`) + SHA-1. The exponent is big-endian with trailing zero bytes stripped by `FUN_13a71ddc`; the wrappers take a 5th arg (exponent length), the core takes `(explen, modbytes)`. **Root RSA-4096 key** constants must be copied in: modulus `0xffff985c` (0x200 B), exponent `0xffff9a5c` (`01 00 01 00`). Root handle `0x0FFFFFFF`. |
| **PRNG** | `0x74`,`0x75`,`0x76` | `0x13a7381c` | `K` = AES(slot 5, IV `01 00…`, 12 zero bytes + rngSeed), computed once. Output block *i* = encryption of 12 zero bytes + `BE32(++counter)`; the IV buffer is reset once per call and chains as CBC; the counter is global and starts at 0 (first value 1). A temporary keyring object is created per call; buffers must be 32-byte aligned. `0x73` GenerateRand (`0x13a73944`) becomes available with it. |

> ECC/RSA/ECDSA are the gate for Step 2 (crypto) and everything that verifies a container, signs,
> or launches a title. Budget this realistically; it is most of Phase 0.

### 0a.1 — ECC + ECDH

Implemented bottom-up from the IOS58 RE, one commit per step, in the source files below. `iosc.c` only gets the thin keyring
glue (step E6).

#### Source layout (follows IOS)

IOS's public-key code comes from six source files, and the call instructions show where each one starts and ends: a call to a
function compiled earlier in the same file is a direct `bl`, every other call is a long call (`ldr` + `blx`, Ghidra's
`COMPUTED_CALL`). For example `FieldInvert` calls `ClearUInt233`/`CopyUInt233` with `bl`, but every caller outside
`0x13a79f00`–`0x13a7ad7f` uses long calls; ECDH calls `HashEccSharedPoint` with `bl` but `EcScalarMultiply` and `LoadUInt233` with
long calls. StarStruck uses the same split:

| IOS file (range) | Contents in IOS | StarStruck file | Exported | Called by |
|---|---|---|---|---|
| F `0x13a79f00`–`0x13a7ad7f` | `ClearUInt233`, `CopyUInt233`, `Wide*`, `MultiplyToWide`, `SquareToWide`, `FieldMultiply`, `FieldFinishInverse`, `FieldInvert`, `HalveUInt233` + tables | `ellipticCurve/uint233.c/.h` (numbers and field arithmetic) and `ellipticCurve/wideUint233.c/.h` (`UInt233Wide`, `Wide*` and the unreduced products `MultiplyToWide`, `SquareToWide`) | `UInt233`, Clear, Copy, Halve, `FieldMultiply`, `FieldInvert`; `UInt233Wide`, `Wide*` and the unreduced products `MultiplyToWide`, `SquareToWide` (`FinishInverse` stays `static`) | everything below |
| D `0x13a76450`–`0x13a76ce3` | `CopyCurvePoint`, `PointAdd/Double/Subtract`, `EcScalarMultiplyPointInPlace`, comb table + comb multiplies | `ellipticCurve/curvePoint.c/.h` | `CurvePoint`, point ops, scalar multiply | A |
| B `0x13a71f38`–`0x13a721d9` | `ReverseWordOrder` (+ `0x13a71f38`), `Load/StoreUInt233(Pair)`, `Load/StoreBigEndianNumber` | `ellipticCurve/uint233.c/.h` (the `UInt233` glue: `ReverseWordOrder`, `Load/StoreUInt233(Pair)`) and `ellipticCurve/bigint_convert.c/.h` (`Load/StoreBigEndianNumber`) | all | C, A; `bigint_convert` also **RSA** |
| A `0x13a70000`–`0x13a706f3` | `EcScalarMultiply` (+ comb variant), `k·G` with on-curve check, hash → number, ECDSA sign/verify cores | `ellipticCurve/ecc.c/.h` | all | C |
| E `0x13a772d0`–`0x13a77db3` | mod-n bignum (RSAREF `nn.c`), modexp | `bigint.c/.h` (later) | all | A, RSA |
| C `0x13a750f8`–`0x13a763cd` | `HashEccSharedPoint`, ECDH (+ comb variant), key generation, private → public, ECDSA and RSA wrappers, `ECC_InitParameters` | `iosc_ecdh.c/.h` (ECDH part) | `ECDH_ComputeSharedSecretHash` | `iosc.c` |

- Dependencies only point down: C → A → D → F, A → E, C/A/RSA → B, and B → F (`ReverseWordOrder` clears with `ClearUInt233`).
- Exported functions use their Ghidra names without the `ECC_` prefix (`FieldMultiply`, `LoadUInt233`, `LoadBigEndianNumber`).
  Unlike IOS, the `UInt233` byte glue lives with `UInt233`; only the generic big-endian helpers, which RSA uses too, are separate.
  Also unlike IOS, IOS's field file F is split in two: the 15-word `UInt233Wide` helpers have their own `wideUint233` file.
- No LTO, so small helpers are not inlined across files. IOS does not inline them either (long calls).
- The rest of IOS's file C (ECDSA/RSA wrappers, the curve constants of `ECC_InitParameters`) arrives with ECDSA/RSA; see decision 3.

**Call tree of syscall `0x61`** (IOS58 addresses, `(size)` in bytes of IOS code; the byte ↔ word helpers and word primitives are left out):

```
IOSC_ComputeSharedKey          0x13a72bf4  ownership checks, stack switch        -> iosc.c (E6)
 └ IOSC_ComputeSharedKey_Inner 0x13a70f18  keyring plumbing, type checks          -> iosc.c (E6)
    └ ECDH_ComputeSharedSecretHash 0x13a75218 (0x68)                              -> iosc_ecdh.c (E5)
       ├ ECC_InitParameters           0x13a75768 (0xc66)  only a=1 and a flag matter here, see below
       ├ ECC_EcScalarMultiply         0x13a700e8 (0x24)   wrapper, returns x only             E4
       │  └ ECC_EcScalarMultiplyPointInPlace 0x13a76780 (0x12a)  signed-digit (NAF) double-and-add  E4
       │     ├ ECC_PointDouble        0x13a7662c (0xea)                              E3
       │     ├ ECC_PointAdd           0x13a76470 (0x1ac)  calls PointDouble if p1 == p2   E3
       │     └ ECC_PointSubtract      0x13a76724 (0x54)   calls PointAdd                  E3
       │         field layer:  ECC_FieldInvert 0x13a7a868 (0x4cc) -> ECC_FieldFinishInverse 0x13a7a68c (0x1dc)   E2
       │                       ECC_FieldMultiply 0x13a7a498 (0x1f4) -> MultiplyToWide / SquareToWide (written)       E1
       └ ECC_HashEccSharedPoint       0x13a750f8 (0x6e)   StoreUInt233 + SHA-1                        E5
```

About 31 functions and 5.2 KB of IOS code; `FieldInvert` (1.2 KB) is by far the largest, then `MultiplyToWide`, `FieldMultiply` and `FieldFinishInverse` (~0.5 KB each).

**Not needed for ECDH** (all of it belongs to the `k·G` users, ECDSA and key generation, later):
- The constants set up by `ECC_InitParameters`: `b`, `G` and the 16-entry comb table. ECDH only reads `a` (`=1`,
  `parameters+4`) and the `Scalar` flag (always `1`, so the write guards in `PointDouble`/`PointAdd` can go).
  The `generator` argument of `ECC_EcScalarMultiply` is unused.
- The comb routines (`FUN_13a7690c`, `FUN_13a76a1c`, `FUN_13a76b38/b80`, `ECC_PointCombTable16`), the on-curve
  check (`FUN_13a70000`) and the mod-n bignum layer.

| Step | File | Scope (IOS functions) | RE status | Deliverable and tests |
|------|------|----------------------|-----------|-----------------------|
| **E1** | `uint233.c` | `FieldMultiply` (0x1f4): reduction by `x^233 + x^74 + 1`; `a == b` takes the square path | confirmed (2506 cases) | Reference compare plus field axioms (commutativity, distributivity, `a·1`). |
| **E2** | `uint233.c` | Inversion: `FieldFinishInverse` (0x1dc) and `FieldInvert` (0x4cc, **inferred**) | partly inferred: `FinishInverse` confirmed, `FieldInvert` inferred | `a · inv(a) == 1` on random values, cross-checked against Fermat `a^(2^233−2)` built from E1 inside the harness. See decision 1. |
| **E3** | `curvePoint.c` | Group law: `CopyCurvePoint` (`0x13a76450`), `PointDouble`, `PointAdd`, `PointSubtract` (4 functions, ~0x300) | `PointDouble` confirmed; `PointAdd`/`PointSubtract` read, not run | Needs the NIST B-233 `b`, `G` **in the tests only**. 2G…16G on the curve (`y²+xy = x³+x²+b`), `P+(−P)` = `(0,0)`, `P+P == Double(P)`, associativity, `(0,0)` as identity. |
| **E4** | `curvePoint.c`, `ecc.c` | `EcScalarMultiplyPointInPlace` (0x12a, **inferred**, in `curvePoint.c`) and the `EcScalarMultiply` wrapper (0x24, in `ecc.c`) | inferred | `k·G` against OpenSSL for random `k` plus `0`, `1`, `n`, `n+1`, `2^233−1` (the scalar is not reduced mod n). `k = 0` returns `(0,0)`. |
| **E5** | `iosc_ecdh.c` | `ECDH_ComputeSharedSecretHash` (0x68) and `HashEccSharedPoint` (0x6e, SHA-1 over the 30-byte `x`) | clean | End to end against OpenSSL (see test strategy). Both directions agree: `a·(b·G) == b·(a·G)`. |
| **E6** | `iosc.c` | Glue in `iosc.c`, not `iosc_ecdh`: `IOSC_ComputeSharedKey` outer and `_Inner`, syscall table entry `0x61` and the `OS*` wrapper | clean | Key-type checks, ownership bitmask test, error codes `-2000`, `-0x7d5`, `-0x7d8`, `-0x7dc`; for a Public+Private key the public part sits at `+0x1e` (the `0x3c` bytes after the 30-byte private part). Then `DepersonalizeTicket` is the first real user. |

Order: E1 → E2 are sequential; E3 needs E1 and E2; E4 needs E3; E5 needs E4; E6 needs E5.

**Behaviour to reproduce (bug-for-bug, from the decompile)**
- **Scalar** (`LoadUInt233`): 30 bytes → word 0 = `BE16 & 0x1ff`, words 1–7 = `BE32`; not reduced mod n. A zero scalar gives `(0,0)`,
  so `x = 0` and the secret is `SHA-1` of 30 zero bytes.
- **Point** (`LoadUInt233Pair`): 60 bytes = `x` (30) ‖ `y` (30), loaded without masking. No on-curve check and no `(0,0)` check, so invalid
  points flow through the arithmetic.
- **Output** (`StoreUInt233`): only `x`, as 30 bytes big-endian (the low 16 bits of word 0, then words 1–7). Secret = `SHA-1` of those 30 bytes.
  Like IOS, the value is shifted left 16 bits into a word buffer and `dst` is written **only through `memcpy`**: byte stores
  to MEM1 trash the whole word, and the core `memcpy` works around that. Keep it that way; the bytes are big-endian because Starlet is.
- **Sizes:** `outSize >= 0x1f` returns `true` and writes nothing. Otherwise `outSize` bytes are copied from a 32-byte local of
  which only the 20 SHA-1 bytes are written. Callers pass 16 (AES-128). See decision 2.
- **Scratch:** IOS keeps `ECC_PointStorage`, the `MultiplyToWide` table (0x3c0) and the parameters (0x444) as globals. That is safe
  there because IOSC calls are serialised by the IOSC mutex.
- `FieldInvert(0)` is never reached: `PointDouble` returns early for `x == 0` and `PointAdd` for `x1 == x2`.

**Stack requirement.** The IOSC code must fit the `0x1000`-byte IOSC stack (`__ioscstack_size`), as in IOS: `0xFFFFE100`–`0xFFFFF100`, entered through
`CopyStackFromStart` (`0xffff5c9c`). The deepest ECDH chain (`ECDH → … → PointAdd → PointDouble → FieldInvert → FinishInverse`) needs about **1.6 KB**
(IOS frames, push + `sub sp`: `ECDH` 188, `EcScalarMultiply` 76, `ScalarMultiplyPointInPlace` 356, `PointAdd` 196, `PointDouble` 184, `PointSubtract` 88,
`FieldInvert` 176, `FieldFinishInverse` 408, `FieldMultiply` 456, `MultiplyToWide` 100, `SquareToWide` 76). Check the real usage of the StarStruck build with
`make -C tests/ecc_test stack` (cross-compiles every ECC file with the kernel's flags from `sdk/starstruck_rules`, merges their call graphs and reports the deepest chain from `ECDH_ComputeSharedSecretHash` against the budget; until E5 it reports that the entry is not defined yet); the later RSA and ECDSA code runs on the same stack. The ECC scratch is not on that stack: `ECC_PointStorage` (`0x13a754c0`) and
`IOSC_ECC_Parameters` (`0x13a76e4c`) are globals in the IOS crypto segment.

**Test strategy**
- The code has no hardware dependency, so the host harness in `tests/ecc_test/` (`make`, not part of the kernel build) builds it
  natively. Every kernel file has its own test file (`uint233_test.c`, `wideUint233_test.c`, `bigint_convert_test.c`, `curvePoint_test.c`, ...) that
  includes that `.c` file, which makes its `static` helpers testable, and has one `Run…Tests` entry point that `ecc_test.c` calls.
  A step adds its tests to the test file of the kernel file it changes (a new kernel file gets a new test file, added to
  `TEST_FILES` in the Makefile). Its Makefile uses `sdk/starstruck_rules` like every other project, so it needs `DEVKITARM` and
  the core headers in `sdk/include`.
- **Byte order:** the load helpers use explicit shifts and work on any host. `StoreUInt233` depends on Starlet being big-endian
  (word buffer + `memcpy`, see above): on a little-endian host the tests swap each 4-byte group back and can only check the first
  28 of the 30 bytes, because the last 2 never reach the output there. The full 30 bytes are covered on Starlet, e.g. by the E5 result.
- **Vectors:** one generator per kernel file, `ref/gen_<name>_vectors.py` → `build/<name>_vectors.inc`, run by the Makefile
  (`gen_uint233_vectors.py`, `gen_wideUint233_vectors.py`, `gen_bigint_convert_vectors.py`). A test file includes only its own vectors: gcc rejects unused
  `static const` arrays under `-Wall -Werror`.
- **E1–E2:** `ref/gf233.py`, a reference over Python big integers (`GF(2)[x] / (x^233 + x^74 + 1)`, the B-233 constants and a plain group law).
- **E3–E5:** OpenSSL supports `sect233r1`; `make vectors` (`ref/gen_vectors.py`) writes ECDH vectors as a C include. `openssl pkeyutl -derive` on two `sect233r1` keys outputs the raw
  shared `x` as exactly 30 bytes, which is what IOS hashes. The expected secret is `SHA-1(x)` truncated to the key size.
- Optional real-hardware vector: a retail ticket plus a console's NG private key through `DepersonalizeTicket`.

**RE to do for this block**
1. `ECC_FieldInvert` and `ECC_EcScalarMultiplyPointInPlace` are inferred from the decompile; the E2/E4 tests have to confirm them. Any correct scalar multiply gives identical results, so the exact structure is not needed.
2. Unreferenced code at `0x13a7a388`–`0x13a7a3ff` (no xrefs, no Ghidra function): confirm it is dead.
3. Purpose of `ECDHParameters.Scalar` (always `1`); only matters if a second mode exists.

**Reuse by the later steps (not in this phase).** `uint233`, `curvePoint`, `ecc` and `bigint_convert` are shared with ECDSA,
key generation and `GetDeviceCertificate`. Those additionally need the `ECC_InitParameters` constants (`b`, `G`, comb table),
the on-curve check (`ecc.c`), the mod-n layer (`bigint.c`) and the PRNG. The comb routines are only a speed optimisation: `k·G`
through the generic E4 gives identical results, so they can be skipped. They also need two more helpers that
ECDH does not: `StoreUInt233Pair` in `uint233.c` (`0x13a72004`, public key `x ‖ y` and the ECDSA signature `r ‖ s`) and
`StoreBigEndianNumber` in `ellipticCurve/bigint_convert.c` (`0x13a7213c`, the inverse of `LoadBigEndianNumber`, also used by RSA). In IOS those write `dst` with
plain byte stores, not `memcpy`; decide whether to copy that or use `memcpy` as `StoreUInt233` does.

**Decisions needed**
1. **Inversion:** port `FieldInvert`/`FinishInverse` faithfully (recommended: matches IOS, results are
   identical either way), or use Itoh–Tsujii (`a^(2^233−2)` from E1, about 40 lines)?
2. **`outSize` above 20:** IOS copies uninitialised stack bytes. Zero-fill, or copy whatever is there?
3. **Rest of IOS file C:** IOS keeps the ECDH, key generation, ECDSA and RSA wrappers and `ECC_InitParameters` in one file. Grow
   `iosc_ecdh.c` into that one file (renamed, e.g. `iosc_pk.c`), or give ECDSA and RSA their own `iosc_ecdsa.c`/`iosc_rsa.c`?
   Decide when ECDSA starts.

### 0b — New syscalls

Add kernel impl + table entry + `OS*` wrapper (decl in `core/include/ios/syscalls.h`, stub in
`core/source/ios/syscalls_asm.s`) for each syscall ES links (veneer table at `0x2010b728`) that
StarStruck lacks.

| # | IOS name | RE state | Change | Needs 0a? |
|---|----------|----------|-------------------|-----------|
| 0x54 | `set_ahbprot` (SetMemoryProtection) | CLEAN | add | no |
| 0x59 | `SetIpcAccessRights` | CLEAN | add | no |
| 0x61 | `IOSC_ComputeSharedKey` | CLEAN | implement (table entry is `SYSCALL_NULL`) | **ECC** |
| 0x6C | `IOSC_VerifyPublicKeySign` | TYPING; decompile drops stack args | add | **RSA+ECDSA** |
| 0x6F | `IOSC_ImportCertificate` | TYPING; no cert struct in Ghidra | add | RSA (+ECDSA for ECC-signed) |
| 0x70 | `IOSC_GetDeviceCertificate` | CLEAN | add | ECC (priv→pub) |
| 0x71 | `IOSC_SetOwnership` | TYPING | add | no |
| 0x72 | `IOSC_GetOwnership` | TYPING | add | no |
| 0x74 | `IOSC_GenerateKey` | TYPING | add | PRNG (ECC priv = 30 raw bytes) |
| 0x75 | `IOSC_GeneratePublicKeySign` | TYPING; decompile drops stack args | add | **ECDSA+PRNG** |
| 0x76 | `IOSC_GenerateCertificate` | TYPING | add | **ECDSA** (signs with NG key, handle 0) |
| 0x78 | `SetProcessPriorities` | CLEAN | add | no |
| 0x79 | `GetProcessPriorities` | CLEAN | add | no |

Syscall numbers: the table is at `0xffff93d0`, which is index 0, so Ghidra's index minus 2 is the real number. IOS handlers: 0x54=`ffff6234`, 0x59=`ffff2ed4`, 0x61=`13a72bf4`, 0x6c=`13a73ad4`, 0x6f=`13a73ef4`, 0x70=`13a740c0`, 0x71=`13a72484`, 0x72=`13a7254c`, 0x74=`13a72b4c`, 0x75=`13a739cc`, 0x76=`13a73fe0`, 0x78=`ffff17f4`, 0x79=`ffff18f0`.

Key semantics to preserve (IOS behaviour):
- **0x54** ES-only (else `-1`). Enable: `HW_MEMMIRR|=8`, `HW_AHBPROT|=0x80000dfe`, `w16 0x0d8b4202=0`.
  Disable: `&=~8`, `&=0x7ffff201`, `w16 0x0d8b4200=0x18`. ES passes TMD `AccessRights` **bit 0**.
  `HW_MEMMIRR` is `0x0d800060` (Ghidra: `SRNPROT`). Registers exist in `hollywood.h`; `ProtectMemory()` is **not** equivalent — write a new helper.
  **RE to do:** `0x0d8b4202`/`0x0d8b4200` (Ghidra: `MEM_PROT_REG`/`MEM_COMPAT`) have unknown meaning; copy the writes literally.
- **0x59** ES-only, one-shot: `-4` if a table with `count!=0` is already set. Rights blob format:
  salt = bits 0–11 (stored as `(~x<<16)|x`), count = bits 12–15, then `count` × 5-bit entries from bit 16, read LSB-first.
  Path hash `h ^= (h<<7)+c+(h>>5)`; access = `count==0 || (h >> table[h%count]) & 1` (guard the `count==0` division). The same check also runs in `RegisterResourceManager`.
  StarStruck's `GetPpcAccessRights` already has the hash; salt/table setters are new.
  After setting, recompute `PpcHasAccess` for every resource manager (38 entries) and write `0x38` to `0x0d80000c`
  (IX1|IX2|Y2, a plain write — needs a new write). ES's default blob is 12
  **zero** bytes (`DAT_2010e540`), so count=0 and it can still be set again later by `DIVerify`.
- **0x71** allowed only if the caller is the **sole** owner (`(owner|me)==me`); new mask =
  `(mask & ~7) | (1<<pid)` → **caller keeps access**; pid 0–2 bits are dropped.
- **0x72** returns the raw owner bitmask; caller must own the key (else `-2000`); root → `-2002`.
- **0x74** `GenerateKey`: a handle below 12 or the root gives `-2000` (checked after `FindKeySize`). The keyring `Zeroes` field is a secrecy
  level: 0 = fused key (never exportable), 1 = software-known, 2 = IOSC-generated (exportable only under a key with 0 or 2). `GenerateKey`
  sets 2 and `ComputeSharedKey` copies it from the private key. HMAC keys get 0x14 random bytes plus zero padding. A Public+Private ECC key
  stores the masked scalar, then the public key.
- **0x6f** `ImportCertificate` (`FUN_13a71668`), offsets from the code (signature at `+4`; type `0x10000/1/2`):
  RSA4096-signed: issuer `0x240`, name `0x284`, pubkey `0x2c8`, exponent `0x3c8`, hash `0x1c0` bytes.
  RSA2048-signed: issuer `0x140`, key type `0x180` (1 = RSA2048, 2 = ECC), pubkey `0x1c8`, exponent `0x2c8`, hash `0x1c0` or `0x100`.
  ECDSA-signed: issuer `0x80`, pubkey `0x108`, hash `0x100`. The key-type field is not checked for types `0x10000`/`0x10002`; only 4 bytes of
  the cert pointer are access-checked; the subject size comes from the handle and a mismatch is not detected.
  Errors: `-0x7db` check failure, `-0x7dc` internal, `-0x7d8` format, `-0x7d5` object type.
- **0x6c** returns 0 without verifying for a Public+Private key that is not ECC (`0x13a71490`). **0x75** always copies `0x3c` bytes.
- **0x70/0x76**: the AP issuer is `Root-CA-MS-NG`, the signature covers `0x80..0x180`, key id 0.
- **0x78/0x79** ES-only. Entry = `{u32 pid, u32 prio}`; `pid>0x13` or `prio>0x7d` → `-4`;
  protected pids `{0,1,2,0xc,0xe,0xf}` → `-1`. Requires a per-process base-priority array.
  Add `ProcessesPriorityArray[20]` and have `CreateThread` record `max(old,prio)` at index
  `(creator pid<=1 && arg<=0x13) ? arg : creator pid` (IOS 0xffff01f8). **0x78** also rewrites every live thread of each listed pid with a
  non-zero new priority: `new - (oldBase - cur)`, applied to both current and initial priority, no requeue. Entries are validated in order:
  `pid>0x13` → `-4`, protected pid → `-1`, `prio>0x7d` → `-4`. **0x79** is an in/out array with no protected-pid check. `LaunchModule` must keep
  passing `UserId` as the thread arg so modules get recorded correctly.

**Phase 0 exit criterion**: a homebrew client can call each new syscall and ECC/RSA/ECDSA verify
known IOS58 test vectors (device cert, a retail ticket/TMD signature) on HW and emulator.

---

## Phase 1 — ES foundations

No ioctlv yet. Build the skeleton, the shared data, and the helper layers everything else uses.

> **Prerequisite**: `modules/es/source` holds only the stub (`es.c`/`es.h`). Bring `types.h`
> (TitleType/TitleID/TitleUIDEntry) and `filesystem/fs.c|h` over from `feature/es` (`8fc3dc2`)
> by merge or cherry-pick before starting 1a/1b.

### 1a — Types (`source/types.h`, extend)
Port the `/ES` structs from Ghidra verbatim:
`TitleType`, `TitleID`, `TitleUIDEntry` (from `feature/es`); add `Ticketv0` (0x2a4), `Ticket` (v0 + v1 header,
0x2b8), `TicketView` (0xd8), `TicketLimit`, `TicketLimitEntry` (0x48), `TitleMetadata`
(0x1e4 + 512·0x24), `TitleMetadataContent` (0x24), `TitleMetadataView` (0x205c),
`TitleMetadataViewContent` (0x10), `ESContext` (0x1c0), `TitleImportExportContext`,
`ActiveTitleContext` (0xc), the certificate family, `SharedContentEntry` (0x1c), enums
`ContainerType {TMD=0,Ticket=1,DeviceCertificate=2}` and `ESErrorCode`.

Layout details to get right:
- **`TitleImportExportContext` is 0x140.** `FreeTitleImportExportContent` memsets
  `0x140`, and `ESContext.active` sits at `0x40+0x140 = 0x180`. Fields: `+0x14/+0x18` are one
  `u64 currentContentDataProcessed` (carry-add in the binary); padding at `+0x94` and `+0xb0`;
  `spareContentBuffer` (64-byte AES carry buffer) at `+0xc0`.
- Cert key modelling: `SignerCert`/`CACert` need the `u32` exponent and
  trailing padding (full sizes 0x300 / 0x400). There is **no** struct for the RSA-2048-signed
  **ECC** cert (the "MS" device cert, 0x240 B); `VerifyContainer` casts it to `SignerCert*`.
  Cert key layout depends on `header.type` (0=RSA4096, 1=RSA2048, 2=ECC).
- `ESErrorCode` needs `-1034` (HasKoreanKey "wrong key") and `-1037`
  (`CheckTitleVersionsAndStuff` sysversion-content-mismatch).
- `TicketView.limits` is `TicketLimit[8]` (the 48 bytes at ticket `0x1f2..0x221` are copied verbatim); `Ticketv0.limits` at `+0x264`.
- `ESErrorCode` also needs `-1016` (no free context, `HandleOpen`) and `-1033` (ticket limit exceeded); `HandleClose` returns `-1017` on an inactive context.

**RE to do (Ghidra DB, not blocking):** `TitleImportExportContext` is 0x10c there (real 0x140: it is `alignas(0x40)`; gaps `0x94`–`0x9f`, `0xb0`–`0xbf`
and `ESContext` `0x08`–`0x3f`/`0x184`–`0x1bf` are alignment padding); `+0x14/+0x18` is one `u64`; `SignerCert`/`CACert` lack the exponent and pad;
the "MS" ECC cert struct (0x240) is missing; `TicketView.limits` is typed `uint[8][2]`; `TitleType`/`TitleID` are not in Ghidra and
`SharedContentEntry` sits in `/` instead of `/ES`; the `Boot2Header` field names are wrong (see Phase 8, `ImportBoot`); `IOS_ios_boot` (1 arg) and
`ES_IOSC_SetData` (void) prototypes are wrong; `SeekContent`'s `whence` is typed `undefined`.

Add `CHECK_SIZE`/`CHECK_OFFSET` for every struct.

### 1b — FS wrapper layer (`source/filesystem/`)
ES has its own thin `/dev/fs` wrappers (`ES_FS_*`, `0x20109e5c`–`0x2010a5a5`) plus higher-level
helpers. Differences from StarStruck's `core/source/fs/fs.c` to reconcile
(bug-for-bug where behaviour is observable):

- **`/dev/fs` fd (`ES_FS_FD`, init -1)**: opened on demand, **not checked**, and **closed +
  reset to -1 after every path op** (except Format/GetStats/Shutdown). StarStruck keeps it open.
  Match IOS (close-after-each).
- **Command buffer**: ES uses a heap-allocated `0x100` buffer (allocated in
  `CreateStartupDirectories`, never freed) and **does not memset** it between calls, so stale
  bytes land in uid/gid/perm fields. Reproduce (bug-for-bug), and verify whether `/dev/fs`
  actually reads those fields (open RE question for the FS module).
- Helpers to port: `CreateDirRecursive` (**re-chowns existing ancestors to 0:0** with the
  caller's perms — reproduce), `CreateAndWriteFile` (`/tmp/` + first 12 chars of basename →
  Delete → CreateFile → Write → Close → mkdir-p of dest dir → Rename; atomic only via the
  rename), `GetTitleUserId` (get-or-create `/sys/uid.sys`; **missing-file quirk**: writes only
  `{1-2 → 0x1000}` and returns `0x1000` for *any* TID; new uid = *last* entry's uid + 1;
  first-match-wins lookup — all reproduce), `DeleteDirectoryIfEmpty` (walk up, never delete a
  first-level dir), `ReadDirectoryEntries`.
- **Dead code, skip**: `FUN_2010a208` (FS GetUsage ioctlv 0xC) and `FUN_2010a5e0`
  (FS MassCreate ioctlv 0xE) — no xrefs in ES.
- **RE to do (FS module):** `/dev/fs` ioctl on a negative fd; recursive directory delete; ReadDir on a file entry; rename of a directory onto an
  existing empty directory (`FinishUpImport`, `RenameTitleImportContentDir`); ReadDir result order (`GetTitles`/`GetOwnedTitles`/`GetSharedContents`
  return it as-is, `ReadDirQuick` assumes 13 bytes per entry). `FS_ENOENT` is `-106`.
- `FS ioctl 10` (SetFileVerCtrl, used on `/tmp/cc.sys`) requires an empty existing file and bumps the NAND generation; StarStruck already has `IOCTL_SETFILEVERCTRL`.
- `GetTitleUID`: if `uid.sys` exists but is empty, the new uid is an uninitialised stack value plus 1.
- Error-code mapping: ES uses `-1024` (ENOMEM), `-1009` (short read), `-1010` (short write),
  `-101`/`-105`/`-106`. Fix StarStruck's ES-side helpers to use the ES codes.

### 1c — Globals & module skeleton (`source/es.c`)
Define the globals (addresses for cross-ref): `Global_ES_MessageQueue` (0x2010d000),
`ES_FS_FD` (0x2010d060), `ES_FSCommandBuffer` (0x2010d064), `ES_ContextArray` (0x2010e000),
default IPC rights blob `DAT_2010e540` (12 zero bytes), `ES_IsNonDiscAuthenticated` (0x2010e5bc),
`ES_ContentFdMap` (0x20112620, `{s32 fd; u32 uid}[16]`), `ES_CurrentActiveTitleContext`
(0x201126a0), `ES_CommonKeyHandles` (0x2010d004 = `{4, 0xB}` — OTP common key, EEPROM/Korean key),
`ES_ZeroKey` (0x2010e580), the Korean-key KAT buffers (0x2010e5c0/e5e0/e600, checks 0x2010d03c/d04c),
and the playtime-timer globals: `ES_TimerId` 0x2010e5a0, state `s32` 0x2010e5a4 (0..2 = interval index, -1 = final countdown, -2 = limit exceeded),
interval `u64` 0x2010e5a8, elapsed `u64` 0x2010e5b0 (never reset), `ES_CurrentPlayTimeSeconds` 0x2010e5b8, interval table `u64[3]` µs
`{30M,60M,300M}` at 0x2010d00c, threshold table `u64[3]` µs `{0,60M,600M}` at 0x2010d024. These are unnamed/mistyped in Ghidra. The static
buffer 0x2010d080–0x2010d17f is dead (replaced by the heap allocation before use); `DAT_201125e8`–`2011261c` are `ES_main` stack locals (stack
0x2010e620–0x20112620).

Write `ES_main` skeleton + `HandleOpen`/`HandleClose`/`HandleIoctlv` dispatch with **every
ioctlv returning `-1017` (stub)**, register `/dev/es`, create the queue and timer. This gives a
loadable module to grow into. Dispatch validation table in
[Appendix A](#appendix-a--ioctlv-dispatch-table).

**Exit criterion**: module loads, `/dev/es` opens/closes, every ioctlv returns `-1017`.

---

## Phase 2 — Crypto / PKI (`source/crypto/`)

Depends on Phase 0a/0b. This unlocks every verify/sign path.

| ioctlv | Function | Addr | Notes |
|--------|----------|------|-------|
| — | `VerifyContainer` | 0x20106b9c | Core PKI. Prefixes: `XS`=Ticket, `CP`=TMD, `MS`=DeviceCert (else -1017). Seq: CreateObject+ImportCertificate(CA, root `0x0FFFFFFF`) → CreateObject+ImportCertificate(signer) → GenerateHash SHA-1 over `[off,size)` (off=0x80 device cert, else 0x140) → VerifyPublicKeySign → optional store update + ImportCertificate → DeleteObject×2. `cert_slot_out` is an **input** handle. `updateCertificateStore` comes from the caller: 1 for ImportTicket, AddTitleStart (TMD), ReimportTitleInit, ExportTitleInit, SetupStreamKey, GetStoredTMDContents; 0 for DiVerifyInner, ImportBoot and AddTitleStart's sys-cert ticket check. |
| — | `CertificateSearchFunction` | 0x201068f4 | Find cert by issuer name in a blob. A name starting `AP` matches any cert whose bare name starts `AP` (`strncmp` 2); `useFullChainName=1` compares `issuer-name` (snprintf into a 0x40 buffer). Size from the signature type: RSA4096 → 0x400 (key type unchecked), RSA2048 → 0x300 (RSA key) or 0x240 (ECC key) else `-1005`, ECDSA → 0x180, other → `-1012`. No match → `-1027` and all three outputs are zeroed. |
| — | `UpdateCertificateStore` | 0x20106a58 | Append/replace in `/sys/cert.sys` (flat concat of var-size certs); it only **appends** (never removes an older entry of the same name); skips only if the found size equals the new size; any open error (not just ENOENT) is treated as an empty store and the file is overwritten; the fd leaks on failure after open and `Close(negative)` is called on exit. Write is `CreateAndWriteFile("/sys/cert.sys", 0,3,3, other=1, …)`. |
| — | `CheckKeyslotPermissions` | 0x2010651c | `GetOwnership(h)` returns a **bitmask** tested with `1<<uid`; handle 6 (SD key) always allowed; else -1017. If `ES_CurrentActiveTitleContext.ticket != 0` it calls `GetTitleUID(ticket->title_id)` (may append to `uid.sys`); a result equal to the caller uid makes uid `0xf`. The test is a register shift `1<<uid`, so uid ≥ 32 gives mask 0. |
| 0x2c | `Encrypt` | 0x20106574 | sync `0x69`. vec0=keyslot, vec1=IV-in (len-checked, unread), vec2=input, **vec3=IV in/out (used)**, vec4=output (len==vec2). vec0 must be 4-aligned. |
| 0x2d | `Decrypt` | 0x201065b0 | sync `0x6b`; same vector layout. |
| 0x30 | `Sign` | 0x20106dd4 | CreateObject(Private ECC)+GenerateKey → GenerateCertificate(ap,"AP%08x%08x",out 0x180) → GeneratePublicKeySign(digest,20,ap,out 0x3c) → DeleteObject. Does **not** write the device cert. vec0 must be 0x40-aligned and an active title context is required, else `-1017`. |
| 0x31 | `VerifySign` | 0x20106f08 | VerifyContainer(DeviceCert) → ImportPublicKey(AP) → VerifyPublicKeySign (ECC). Copies the certs into an aligned heap buffer and first verifies the AP cert's signature with the NG key handle (hash from `+0x80`, size-0x80) before the data check. |
| 0x1e | (inline) `IOSC_GetDeviceCertificate` | 0x2010b488 | Thin `0x70`. vec0 len 0x180. |

`HandleIoctlv` shows the in/out counts as `open_cmd.mode/uid`: mode 3 / uid 2 means in=3/out=2.

**RE to do:** none beyond Phase 0 (`ES_Decrypt` params are untyped; `ES_Encrypt`'s `data_enc/data_dec` are really input/output).

**Verify**: sign/verify a known blob; import the device cert; round-trip encrypt/decrypt against
IOS58.

---

## Phase 3 — TMD (`source/tmd/`)

TMD path `"/title/%08x/%08x/content/title.tmd"`. **Stored size = file length** (not the
`0x1e4+n·0x24` formula — that formula is only for in-memory/IPC-length checks).

| ioctlv | Function | Addr | Notes |
|--------|----------|------|-------|
| 0x34/0x35 | `GetStoredTmd` | 0x2010710c | size query returns file size; buffer too small → -1017; FS errors pass through. |
| 0x14/0x15 | `GetTitleMetadataView` | 0x2010738c | does its **own** open/read (does **not** call GetStoredTmd); no sig check. |
| 0x19/0x1a | `GenerateTMDView` | 0x201071f0 | `tmd==NULL`(+len 0) → active title's TMD. View **contains** trimmed content records (0x10 each); size `0x5c+0x10·n`. Copy map in Appendix B. |
| 0x39/0x3a | `GetActiveTitleTMD` | 0x2010733c | from `ES_CurrentActiveTitleContext`; needs `active==1`. |
| 0x10/0x11 | `GetTitleContents` | 0x2010819c | from stored TMD; NULL array = count-only. |
| 0x32/0x33 | `GetStoredTMDContents` | 0x20108280 | from caller-supplied TMD, **VerifyContainer'd** (store update=1 — may write cert.sys). |
| — | `GetStoredTMDContentsInner` | 0x20108030 | shared content counted only if its SHA-1 is in content.map; private counted only if its `.app` opens; last-error-wins quirk; output written even on error (reproduce). |

`GetStoredTMDContents` and `SetupStreamKey` call `VerifyContainer` with `update=1`, but their certs are found inside the `cert.sys` buffer that was just read, so
`UpdateCertificateStore` finds the same name and size and writes nothing. `GetStoredTMDContentsInner` calls `IOS_Close(fd)` even when `fd < 0`.

**RE to do:** none.

---

## Phase 4 — Ticket (`source/ticket/`)

v0 at `/ticket/<hi>/<lo>.tik`; **v1 at a separate `.tv1` file** (not appended).
Both hold multiple records; same-`ticket_id` replaced in place else appended (via tmp+rename).
System titles `1-2..1-0x101` overwrite with a single ticket.

| ioctlv | Function | Addr | Notes |
|--------|----------|------|-------|
| 0x01 | `ImportTicket` | 0x201020bc | Depersonalize (only when `device_id != 0`) → VerifyContainer(XS, on the *depersonalized* ticket, store update) → ImportV0V1. `HandleIoctlv` passes only v0 and v1; v2 is never read. |
| — | `DepersonalizeTicket` | 0x20101a18 | Requires `GetData(1)==device_id` (else -1020). **Uses ECDH**: ImportPublicKey(ticket+0x180 as ECC) → ComputeSharedKey(handle 0 NG, pub, aes) → AES-CBC-decrypt `title_key` (IV = `ticket_id‖0`) in place. Does **not** blank device_id. |
| — | `ImportV0Ticket`/`ImportV1Ticket`/`ImportV0V1Ticket` | 0x20101b60 / 0x20101dcc / 0x201020a0 | version byte 0x1bc selects the file; v1 record = `0x2a4 + ticket_size`. A ticket whose title_id low word is 0 lands in `/ticket/<hi>/00000000` — that is how "super tickets" are written (no special writer). |
| 0x12/0x13 | `GetTicketViews` | 0x201078b8 | v1 views only if 0 v0 views; also reads the `/ticket/<hi>/00000000` "super tickets". |
| 0x1b | `GenerateTicketView` | 0x20107478 | **never copies `common_key_index`** (view+0x25 stays 0) — reproduce; must be byte-exact (OpenTitleContent memcmps it). |
| 0x40 | `GetV0TicketFromView` | 0x20107974 | |
| 0x43/0x44 | `GetTicketFromView` | 0x20107d30 | 0x43 size / 0x44 data; v0 vs v1 from `view.version`. v0 size query returns 0x2a4 with no lookup; v1 size query needs the ticket + an access check. |
| — | `GetV0TicketFromViewInternal` | 0x20105de8 | used by `DiVerifyWithTicketView` and `SetupStreamKey` (not by `DIVerify`). |
| 0x18 | `DeleteTicket`→`DeleteV0/V1Ticket` | 0x201090dc / 0x20108ba8 / 0x20108e0c | deletes `.tik`/`.tv1`; `CanDeleteTitle` TID gate only. |
| 0x16 | `GetConsumption` | 0x2010229c | input is a **ticket_id**; returns stored `(type,consumed)` pairs from `/sys/cc.sys` (not "remaining"). NULL buffer → count 8 if entry exists. |
| 0x21 | `SetUID` | 0x20105790 | caller uid must == uid of title 1-2 (else -1017); `GetTitleUID`(target, allocates in uid.sys); `SetUid/SetGid(**pid 15**, …)`, return ignored. |
| 0x45 | `HasKoreanKey` | 0x201096b8 | KAT: Encrypt 16×0xFF with handle 11, IV 0; ==Check1 (the real Korean key's output) → 0, ==Check2 (`3f5b8cc9ea855a0afa7347d23e8d664e` = AES-128-CBC under an all-zero key) → -1034, else -1032. The `do{}while(<0x10)` loop is an empty delay: port it as nothing. |
| — | ticket-limit engine | 0x20102190–0x2010292d | `limits[8]` at +0x264; `/sys/cc.sys` keyed by ticket_id; `cc.sys` entries never removed. See timer section. |

**Playtime timer** (lives in `es.c` tick + ticket engine): timer created unarmed; `ActivateTicketLimits`
arms it with `RestartTimer(I,I)` where `I = min(30s, remaining µs)`; each `0xdeaddead` tick adds `I`
to elapsed and `UpdatePlaytimeLimit` **adds** `I/1e6` s to cc.sys (not decrement). Interval table
`{30,60,300}s` @0x2010d00c selected by elapsed thresholds `{0,60,600}s` @0x2010d024; System Menu
(1-2) launched when consumption ≥ limit+300 (type 1) or ≥ limit+1 (type 4); limit types 0/3 = no limit.

State machine (`ES_main` tick 0x201015e8, `UpdateTicketLimits` 0x201023cc, `ActivateTicketLimits` 0x20102788): state `0x2010e5a4` is 0..2 (index into the
interval table), -1 (final countdown: interval = remaining, then 300 s grace) or -2 (limit exceeded: `cc.sys` is no longer rewritten); -1 and -2 are sticky
until the next type-1 Activate sets 0. Elapsed (`0x2010e5b0`) is never reset and thresholds match by **exact equality**. The timer is periodic
(repeat = I); only the tick calls `StopTimer`, Activate calls `RestartTimer(I,I)` without a stop. The kernel returns `-4` when the timer is already
armed (IOS `ffff43dc`); StarStruck's `timer.c` returns 0 there, which is harmless because ES ignores the result. `IsTicketLimitExpired` sets
`ES_CurrentPlayTimeSeconds` only when a `cc.sys` entry exists, so it can be stale. `UpdatePlaytimeLimit`'s `ms` parameter is microseconds.
`cc.sys` is a headerless array of 0x48-byte `TicketLimitEntry {tid[2], {type,value}[8]}`; only types 1 and 4 are stored, types 2 and 5+ count as
"has limits" but are never evaluated. `GetTicket` falls back to `/ticket/<hi>/00000000.tik` on any error; `ReadTicket` reads only v0 `.tik` files; the
names of their (hi, lo) parameters are swapped in Ghidra.
`GetV0TicketFromView`/`GetV1TicketFromView`: the `(can_export & 0xf) == 5` test is just the literal predicate, no further meaning.

**RE to do:** none.

---

## Phase 5 — Title management (`source/title/`)

| ioctlv | Function | Addr | Notes |
|--------|----------|------|-------|
| 0x0e/0x0f | `GetTitles` | 0x20107eec | pure dir walk of `/title`+`/title/<hi>`; no TMD/ticket filter. **Off-by-one** write past buffer in list mode (reproduce): the check is `out_index <= *count` (`0x20107f7a`), so one extra entry writes `titles[N]` and returns 0; two or more extra write `titles[N]` and then return `-1017`. |
| 0x0c/0x0d | `GetOwnedTitles` | 0x20107d70 | dir walk of `/ticket/<hi>`; entries with `.tik`/`.tv1` (strstr); **no caller check**, list mode **unbounded** write (reproduce). |
| 0x36/0x37 | `GetSharedContents` | 0x2010836c | reads `/shared1/content.map`; record = `{char name[8]; u8 sha1[20]}` (0x1c). OOB map scan bounded by `.app` count (reproduce). |
| 0x38 | `DeleteSharedContent` | 0x2010882c | input = SHA-1; refuses if any **tid_hi==1** title references it; deletes `/shared1/<name>.app`; prunes **all** stale map entries. |
| 0x17 | `DeleteTitle` | 0x20108774 | `CanDeleteTitle` (TID gate only) → recursive delete `/title/<hi>/<lo>` → prune empty `/title/<hi>`. **Tickets not touched.** |
| 0x22 | `DeleteTitleContents` | 0x2010857c | delete content/ entries whose name contains "app". |
| 0x3e | `DeleteTitleContent` | 0x20108660 | one `.app` by CID. |
| 0x1d | `GetDataDirectory` | 0x201090f4 | formats `/title/%08x/%08x/data` only (no mkdir); returns 0 (the decompile's `return tid_hi` is an artifact). |
| — | `CanDeleteTitle` | 0x20108548 | protects `1-1..1-0x101` (unsigned 64-bit range check); **no uid check**. |
| — | `GetSharedContent` | 0x20104e04 | hash lookup in content.map; 3rd out = the first position `i` where `id[i] != i` (assumes a sorted, gap-free map; the count if none differ), used by `AddContentStart`/`AddContentFinish` as the new CID. A missing map returns `-106` with `*out=0`; other open errors leave `*out` untouched. |

`GetTitles`/`GetOwnedTitles` call `IOS_Free(0, list)` unconditionally per subdirectory (so with NULL for an empty directory); `StarStruck`'s `Free(NULL)`
must return an error harmlessly. `DeleteSharedContent` writes `/shared1/content.map` through `CreateAndWriteFile` (temp file `/tmp/content.map`, data = the kept
records, size `0x1c * kept`). `CanDeleteTitle` is `TitleType`-independent (TID range only).

**RE to do:** see FS items in Phase 1b (ReadDir order, recursive delete).

---

## Phase 6 — Content access (`source/content/`)

Uses `ES_ContentFdMap` (`{fd,uid}[16]`, `.fd=-1` init in CreateStartupDirectories).

**RE to do:** `OpenTitleContent`, `CurrentTitleCanAccess` and `SetupStreamKey` read `ES_CurrentActiveTitleContext.tmd->TitleId` without checking that an active title exists (`tmd` is NULL until a title is loaded; NULL-page read at `+0x18c`). Confirm that ES boot always sets the active TMD before the PPC can call `0x24`/`0x3c`; otherwise decide how to handle the fault (Appendix C stop rule).

| ioctlv | Function | Addr | Notes |
|--------|----------|------|-------|
| 0x09 | `OpenContent` | 0x20105140 | active title's TMD resolves index→file; calls `GetTitleUID` for the active title (can write `uid.sys`) — port after Phase 4's `GetTitleUID`. |
| 0x24 | `OpenTitleContent` | 0x201051f0 | view must memcmp-equal an installed view; access via `CurrentTitleCanAccess` (`tid_lo != 0 && (tid_lo & ~permitted_title_mask) == permitted_title_id`) or SysMenu-uid/uid0/HAB special; reads title.tmd **unverified**. |
| 0x0a | `ReadContent` | 0x20105444 | |
| 0x23 | `SeekContent` | 0x20105494 | **first arg is `ctx->uid`** (disasm; decompile shows `vectors`). |
| 0x0b | `CloseContent` | 0x201054e4 | |
| 0x3c | `SetupStreamKey` | 0x201065ec | title key from the **NAND v0 ticket** (not the view); VerifyContainer TMD+ticket; CreateObject(AES)+SetOwnership+ImportSecretKey(CommonKeyHandles[idx]); handle written into caller vec2. Leaks the TMD copy + object on error (reproduce); cleanup closes the fd only if `fd > 0` (`0x2010686c`). TitleType must have bit `0x04` or `0x20`, not both; uid must be `> 0xfff` or `0x13` (WFS); bit `0x20` requires uid `0x13`. For uid `0x13` the ticket-view access check is `(0x13 & ~permitted_title_mask) == permitted_title_id`, other uids use `CanAccess`; the ownership mask is `1<<min(uid,0xf)` when uid `> 0xfff`. |
| 0x3d | `DeleteStreamKey` | 0x201068e8 | DeleteObject; no ownership check. |
| — | `OpenTitleContentInner` | 0x20104fb8 | returns the map slot; all 16 slots in use → `-109` (FS_ENFILE, not in `ESErrorCode`). **Shared path doesn't check the FS_Open result**: a negative fd is stored and a slot index is returned as success, later reads use the negative fd; an fd of exactly -1 leaves the slot looking free (reproduce). |

---

## Phase 7 — Title install / export (`source/title_import/`)

Most complex subsystem. `TitleImportExportContext` embedded at `ESContext+0x40`.
Install flow `0x02→0x03→(0x04)*→0x05→0x06`; failure → `0x2f` cancel.

`ExportContentDataInner` pads its encrypt input from uninitialised heap, up to 31 bytes past the allocation.

**RE to do:** FS rename-onto-existing-empty-directory semantics (see Phase 1b); `ChooseExportKeyForTitle` NULL-TMD behaviour (Phase 6). `TitleImportExportContext` is 0x140 with no unknown fields.

NAND layout: staging in **`/tmp/`** and **`/import/`**, not `/tmp/<hi>/<lo>/…`.
- `WriteTempTmd` → `/tmp/title.tmd`.
- content → `/tmp/%08x.app` (or `/tmp/%s.app` for shared, name = first free shared ID).
- `AddContentFinish` → `/import/<hi>/<lo>/content/%08x.app` (or `/shared1/%08x.app` + content.map insert).
- `AddTitleFinish` → `/tmp/title.tmd` → `/import/<hi>/<lo>/content/title.tmd`, then `RenameTitleImportContentDir` commits `/import/<hi>/<lo>/content` → `/title/<hi>/<lo>/content`.

| ioctlv | Function | Addr | Notes |
|--------|----------|------|-------|
| 0x02 | `AddTitleStart` | 0x201031ac | **hidden 8th arg** `*(u32*)vec3.data` (dispatcher passes it; unused in IOS58 — Appendix D). VerifyContainer(TMD, store) + version checks; ticket = first non-expired v0 from NAND; CreateObject(AES128)+ImportSecretKey(flag1, CommonKeyHandles[idx], IV=`TitleId‖0`); idx≥2 → -1029; refuses 1-1; calls rollback `FUN_20104264` first; sets `ready=1`. Failure paths do not free the context (leak). |
| 0x03 | `AddContentStart` | 0x20103498 | GenerateHash init; returns raw FS fd to PPC; `/tmp/%08x.app`. |
| 0x04 | `AddContentData` | 0x2010394c | → `AddContentDataInner`: sync Decrypt (0x6b) ≤0x4000 chunks via 64-byte carry buffer; GenerateHash update. |
| 0x05 | `AddContentFinish` | 0x20103a54 | finalize hash, compare vs TMD content SHA-1 (−1022 on mismatch); rename tmp→final. |
| 0x06 | `AddTitleFinish` | 0x2010402c | VerifyFSAttr; commit dirs; always frees context. For titles 1-1..1-0x101 every non-optional content (`Type & 0x4000 == 0`) must be present (shared: in `content.map`; else the `.app` in `/import`), else `-1017`. A `-106` from the tmp-TMD rename is tolerated. |
| 0x2f | `AddTitleCancel` | 0x20104470 | abort; fd leak if a content file is open (reproduce). |
| 0x2b | `ReimportTitleInit` | 0x20106068 | **not** "re-verify": starts an import from a caller TMD with no ticket; key from `ChooseExportKeyForTitle`; writes `/tmp/title.tmd`. Refuses the whole 1-1..1-0x101 range (AddTitleStart refuses only 1-1). `VerifyContainer` is called with `update=1`. Community "AddTMD" = 0x2b then 0x06. |
| 0x26 | `ExportTitleInit` | 0x201061ec | no busy check (can clobber an import). Refuses 1-1..1-0x101; `VerifyContainer` with `update=1`. |
| 0x27 | `ExportContentStart` | 0x20106370 | returns an `ES_ContentFdMap` cfd (via `OpenTitleContentInner(tmd, index, uid 0)`), not a raw fd; resets content state before checking `ready`; its error path frees the whole context. |
| 0x28 | `ExportContentData` | 0x20106448 | → `ExportContentDataInner`: sync Encrypt (0x69) with BACKUP_KEY (5) or zero key (DLC range); round up to 32 B (writes up to 31 stale bytes — reproduce); **OOB write on invalid cfd** (reproduce, Appendix C). |
| 0x29 | `ExportContentEnd` | 0x201064b8 | |
| 0x2a | `ExportTitleDone` | 0x20106500 | doesn't close the export fd (reproduce). |
| — | `ChooseExportKeyForTitle` | 0x20102a94 | BACKUP_KEY(5), and no DLC test at all when the ACTIVE title is 1-2. Zero key only when `TitleType & 0x18 == 0x18` and `(tid\|0xff)` is `0x00010005_735841ff` or lies in `[0x00010005_735a41ff, +0x700]`. Dereferences `ES_CurrentActiveTitleContext.tmd` unchecked (see Phase 6 RE to do). |
| — | `FinishStaleImports` | 0x20105538 | startup: walks `/import/<hi>/<lo>` (not /tmp); commits or deletes. |
| — | `FinishUpImport` (`ES_FinishUpImport??` 0x20102fac) | 0x20102fac | `AccomodateNewTitle` plus a rename of an existing `/title/..content` to `/import/..content` when `title.tmd` exists; it renames onto the empty dir `Accomodate` just created, and `RenameTitleImportContentDir` does the reverse. Needs the FS rename-onto-empty-dir semantics (Phase 1b RE to do). |
| — | `FUN_20104264` | 0x20104264 | **rollback** (name it): deletes `/tmp/title.tmd`; restores or removes `/import` dirs. |
| — | `CheckTitleVersionsAndStuff` | 0x20102c10 | anti-downgrade ("system titles" = 1-1..1-0x101: full version; others: `version&0xff00`); SysMenu required-IOS checks; boot2 version check only for IOS 1-3..1-0xff. Returns -1035/-1036/-1037/-1031: -1036 = no ticket for the required IOS, -1037 = the required IOS has contents missing on disk (`GetTitleContents` counts files present). No downgrade flag, no boot-index check. |

---

## Phase 8 — DI verification (`source/diverify/`)

| ioctlv | Function | Addr | Notes |
|--------|----------|------|-------|
| 0x1c | `DIVerify` | 0x20105bd0 | `ctx->uid==3` only. DiVerifyInner → `TriggerActiveTitleAccessRights(false)` → if `IsNonDiscAuthenticated==0` **and** sysversion > 1-0x1B, write `/sys/disc.sys` (`{u64 tid; TicketView 0xd8; TMD}`) when sysver.hi > 1, or hi == 1 and lo > 0x1b and lo != 0xff; if flag==1, `SetIpcAccessRights(tmd->IpcMask)` and clear it. May `WriteTMDForNewTitle`, which (like the gid) uses the caller's unverified TMD buffer. Skipped for 1-1..1-0x101. |
| 0x3b | `DiVerifyWithTicketView` | 0x20105f2c | view variant; uses `GetV0TicketFromViewInternal`. |
| — | `DiVerifyInner` | 0x20105848 | VerifyContainer(ticket+TMD); `hashes[i]=contents[i].Sha1Hash` (no 0x77 call); CreateObject(AES)+SetOwnership(1<<3)+ImportSecretKey(CommonKeyHandles[idx], IV=`TitleId‖0`); fills `ES_CurrentActiveTitleContext`. |
| 0x1f | `ImportBoot` | 0x20109740 | boot2 installer (no DIVerify/launch). tid must be 1-1; content length must equal `round16(contents[0].Size)`; anti-downgrade compares `tmd->RequiredSystemVersion` (low word) with BOOT2_VERSION (not TitleVersion); VerifyContainer ticket+TMD; writes header+certs+ticket+TMD+blob+content to `/dev/boot2`; decrypts a copy with common key handle 4 to SHA-1 (−1022 on mismatch); boot2 ioctl1 → `/tmp/boot.sys`→`/sys/boot.sys` → ioctl3 → delete → `IOSC_SetData(7, sysver_lo)`. Writes uninitialised heap padding (reproduce). Argument map: v0 ticket, v1 ticket certs, v2 TMD, v3 TMD certs, v4 blob, v5 encrypted content (u64 length, hi = 0). Boot2 header: `+0 0x20`, `+4 content_offset = align64(0x20+certs+0x2a4+tmd)`, `+8` certs (0xa00/0xe00), `+0xc 0x2a4`, `+0x10 tmd_size`, `+0x14/+0x18 0`, `+0x1c` blob len; the blob is written after the TMD but excluded from `content_offset`, so the layout is coherent only for an empty vec4. The encrypted input is written to `/dev/boot2`; only a copy is decrypted for the SHA-1. The `boot.sys` journal holds `content_offset+len`; the final call is `IOSC_SetData(7, tmd+0x188)`. The local at `sp+0x64` is never zeroed, so early error paths call `IOSC_DeleteObject(garbage)` (reproduce or zero it). `/dev/boot2`: writes are a page-buffered stream; ioctls 1 and 2 both flush/erase/write 3 blockmap copies (later writes `-4`); ioctl 3 takes a u32 size and writes the second copy, needing `page_cursor == 0`; `FinishBoot2Update` replays ioctl 3 from `/sys/boot.sys`. |

**RE to do:** none. (The Ghidra `Boot2Header` field names are wrong; use the layout above.)

---

## Phase 9 — Launch (`source/launch/`)

| ioctlv | Function | Addr | Notes |
|--------|----------|------|-------|
| 0x08 | `LaunchTitle` | 0x20104cf4 | **4 args** (view + hidden). reset ctx → CheckLaunchSys → GetActiveTitleBootContentPath → GetTitleUID → `SetUid(15,uid)`,`SetGid(15,GroupID)`. IOS titles 1-3..1-0x101: `TriggerAccessRights(1)` + `ios_boot(path, p4, ((tid_lo&0xff)<<16)|TitleVersion)` (`TitleVersion` = `tmd+0x1dc`). Else: `TriggerAccessRights(1)` + `StartPPC(path)` + `ActivateTicketLimits`. `p4`=1 only from WriteLaunchAndReloadIOS (the 4th arg and `LaunchTitleInternal`'s hidden u8 are 0 in every other caller). Veneers: `StartPPC` = syscall 0x41, `ios_boot(path, suspendBroadway, version)` = 0x42, `IOS_WhichKernel` = 0x47 (returns type 3, so `CheckLaunchSys` takes the launch.sys path), `SetPPCACRPerms` = 0x54. On failure `ios_boot` leaves the IPC thread suspended (kernel `ffff1370`); on success it never returns. |
| 0x25 | `LaunchBC` | 0x20101288 | only if `GetBusSpeed()!=0xa2`. |
| 0x41 | `GetProcessPriorities` | 0x20109698 | → syscall 0x79. |
| 0x42 | `SetProcessPriorities` | 0x20109678 | → syscall 0x78. |
| — | `CheckLaunchSys` | 0x201049c4 | gatekeeper: load ticket by id, load TMD, tid check, limit check, boot-content exists+accessible, `active=1`. launch.sys rendezvous for **PPC** titles; IOS titles need `sysversion.hi==0 && lo<=BOOT2_VERSION` else -1031. |
| — | `TriggerActiveTitleAccessRights` | 0x20104c3c | `0x54` with `AccessRights` bit 0; if arg true also `/dev/di` ioctl 0x8E with bit 1 (`checkDVDPerms` → "notify DI"); a failed `/dev/di` open returns 0 (silent success). Does **not** call 0x59. `active!=1` → -1017. |
| — | `LoadThingyIntoTitleContext` | 0x201044dc | type 0 = ticket, 1 = TMD. |
| — | `GetTitleBootContent`/`GetActiveTitleBootContentPath` | 0x20104674 / 0x20104724 | ticket access bit LSB-first at ticket+0x222. The cid is found by `contents[i].Index == BootIndex`, but Type and SHA-1 use `contents[BootIndex]` by array position with no bounds check (reproduce). |
| — | launch files | 0x20104800–0x201049bd | `space.sys` placeholder = **0x4ac4 zero bytes** (not 8); `CreateLaunchFile` writes launch.sys/disc.sys `{tid, view 0xd8, [TMD]}`; `DeleteLaunchFile` recreates space.sys. |
| — | `LaunchTitleInternal` | 0x201095f4 | **3 args** (hidden u8). used by SysMenu/BC/MIOS and `UpdateTicketLimits`; leaks its `views` buffer on success (reproduce). |

**RE to do:** `/dev/di` ioctl `0x8e` semantics (no DI module in the Ghidra DB); confirm StarStruck's `LoadBinary`/`LoadKernel` match `StartPPC` / `ios_boot` (suspend flag, thread state on failure). `ES_main` aborts, and ES stops serving, on any open error other than `-106` when reading `launch.sys`/`disc.sys`.

---

## Phase 10 — Priorities & misc inline (`source/es.c`)

Finish the inline ioctlv handlers in `HandleIoctlv`:
`0x07` GetDeviceID (`IOSC_GetData(DEVICE_ID)`), `0x20` GetTitleID (active TMD),
`0x2e` GetBoot2Version (`IOSC_GetData(BOOT2_VERSION)`), and wire `0x41`/`0x42` to the
Set/GetProcessPriorities wrappers. Full validation per [Appendix A](#appendix-a--ioctlv-dispatch-table).
At this point all 69 ioctlv cases are live.

**Verify**: full `/dev/es` command sweep from a homebrew client vs IOS58 (HW + emulator);
install a WAD, read it back, launch a channel.

---

## Phase 11 — Boot-flow handover (final)

Replace the temporary boot path in [kernel/source/main.c:270+](../../kernel/source/main.c#L270)
with the IOS58 model, and enable the `ES_main` startup branch:

- `CreateStartupDirectories` (0x201012b8): the 9-entry dir table (Appendix B), special
  `SetAttr` for `/meta` (owner = uid of 1-2, gid 1) and `/wfs` (uid/gid 0x13); allocate the
  FS command buffer; **format NAND if GetStats returns -104**; init `ES_ContentFdMap`; call
  `FinishStaleImports`.
- Kernel flavor ≥ 3 path: `LaunchTheRestOfModules((GetLoMemOSVersion()>>16)&0xff)` — reads
  `/title/00000001/<IOS>/content/title.tmd`, `LaunchRM`s every content except the boot index
  (shared via `/shared1/%s.app`), aborts on first error, ends with `SetThreadPriority(0,0x50)`.
- Then `/sys/launch.sys` (→ read tid+view, GetStoredTmd, `SetIpcAccessRights(tmd->IpcMask)`,
  LaunchTitle) → else `/sys/disc.sys` (→ read, SetIpcAccessRights, delete) → else
  `SetIpcAccessRights(zero blob)` + `IsNonDiscAuthenticated=1`.
- Kernel flavor < 3 path: `FinishBoot2Update` then `GetBusSpeed()==0xa2 ? LaunchMIOS :
  LaunchSystemMenu` (dead on IOS58, but port for completeness).

The kernel's job shrinks to: bring up hardware, start AES/SHA/IPC threads, launch the modules
embedded in the kernel ELF (incl. ES), and let ES drive the rest. Remove the `f_mount`/`boot2_run`
scaffolding and the kernel's unused `sdhc.c`/`sdmmc.c`/`diskio.c`/`ff.c` SD path if it is only
there for that temporary boot.

**Verify**: cold boot to System Menu on HW and emulator; IOS reload via launch.sys; disc boot.

---

## Dependency graph

```
Phase 0  kernel (crypto → syscalls)      ← gates everything
  └ Phase 1  foundations (types, FS, globals, skeleton)
      ├ Phase 2  crypto/PKI        (needs 0a/0b)
      │   ├ Phase 3  tmd
      │   ├ Phase 4  ticket        (needs crypto)
      │   │   └ Phase 5  title mgmt
      │   ├ Phase 6  content
      │   └ Phase 7  import/export (needs tmd, ticket, crypto)
      ├ Phase 8  diverify          (needs ticket, crypto, active-title ctx)
      └ Phase 9  launch            (needs tmd, ticket, 0x54/0x59/0x78/0x79)
          └ Phase 10 inline + priorities
              └ Phase 11 boot handover (needs 9)
```

Recommended order: **0 → 1 → 2 → 3 → 4 → 5 → 6 → 7 → 8 → 9 → 10 → 11.**
Within each ES phase, implement and verify one ioctlv at a time.

---

## Appendix A — ioctlv dispatch table

`HandleIoctlv` reads the vector array from `ioctl_cmd.buffer_out` and the in/io counts from
`open_cmd.mode`/`open_cmd.uid` (a Ghidra union mis-selection — they are really `argcin`/`argcio`).
Inactive context → `-0x3f9`. Unknown/`0x3f` → `-0x3f9`. Alignment is checked **per case**, not globally. No alignment check at all: `0x01, 0x06, 0x1b (v0), 0x1c, 0x1e, 0x25, 0x2a, 0x2b, 0x2f, 0x30, 0x31, 0x3b, 0x41, 0x42, 0x45`. Partial: `0x02` v3 only; `0x1c`/`0x3b` v3 only; `0x1f` v2 only; `0x2c`/`0x2d` v0 only; `0x3c` v1 only (the TMD); `0x1d` v0 only; `0x0a` leaves v1 unchecked. Port the per-case list from the decompile, in the same order, since the order decides the error code. `0x41`/`0x42` never validate `v1.len` against `count*8` (count up to 0x14), so the Get write can overflow.

| Cmd | Handler (IOS58 addr) | in/io | Checks beyond alignment (`v<n>` = ioctlv vector n) |
|-----|----------------------|-------|-----|
| 0x01 | ImportTicket 0x201020bc | 3/0 | v0.len ≥ 0x2a4 and == 0x2a4 (+ `ticket_size` if v1) |
| 0x02 | AddTitleStart 0x201031ac | 4/0 | v3.len == 4 (8th arg `*v3`, unused in IOS58) |
| 0x03 | AddContentStart 0x20103498 | 2/0 | v0.len == 8 (tid), v1.len == 4 (cid) |
| 0x04 | AddContentData 0x2010394c | 2/0 | v0.len == 4 (cfd) |
| 0x05 | AddContentFinish 0x20103a54 | 1/0 | v0.len == 4 (cfd) |
| 0x06 | AddTitleFinish 0x2010402c | 0/0 | – |
| 0x07 | GetDeviceID (inline `IOSC_GetData(DEVICE_ID)`) | 0/1 | v0.len == 4 |
| 0x08 | LaunchTitle 0x20104cf4 | 2/0 | v0.len == 8, v1.len == 0xd8 |
| 0x09 | OpenContent 0x20105140 | 1/0 | v0.len == 4 (index) |
| 0x0a | ReadContent 0x20105444 | 1/1 | v0.len == 4 (cfd) |
| 0x0b | CloseContent 0x201054e4 | 1/0 | v0.len == 4 |
| 0x0c | GetOwnedTitles (count) 0x20107d70 | 0/1 | v0.len == 4 |
| 0x0d | GetOwnedTitles | 1/1 | v0.len == 4, v1.len == n*8 |
| 0x0e | GetTitles (count) 0x20107eec | 0/1 | v0.len == 4 |
| 0x0f | GetTitles | 1/1 | v0.len == 4, v1.len == n*8 |
| 0x10 | GetTitleContents (count) 0x2010819c | 1/1 | v0.len == 8, v1.len == 4 |
| 0x11 | GetTitleContents | 2/1 | v0.len == 8, v1.len == 4, v2.len == n*4 |
| 0x12 | GetTicketViews (count) 0x201078b8 | 1/1 | v0.len == 8, v1.len == 4 |
| 0x13 | GetTicketViews | 2/1 | v2.len == n*0xd8 |
| 0x14 | GetTitleMetadataView (size) 0x2010738c | 1/1 | v0.len == 8, v1.len == 4 |
| 0x15 | GetTitleMetadataView | 2/1 | v2.len == *v1 |
| 0x16 | GetConsumption 0x2010229c | 1/2 | v0.len == 8 (ticket id), v2.len == 4, v1 NULL or v1.len == n*8 |
| 0x17 | DeleteTitle 0x20108774 | 1/0 | v0.len == 8 |
| 0x18 | DeleteTicket 0x201090dc | 1/0 | v0.len == 0xd8 |
| 0x19 | GenerateTMDView (size) 0x201071f0 | 1/1 | v0 NULL+len 0 (→ active TMD) or len == TMD size; v1.len == 4 |
| 0x1a | GenerateTMDView | 2/1 | as 0x19, v2.len == *v1 |
| 0x1b | GenerateTicketView 0x20107478 | 1/1 | v0.len == 0x2a4 or 0 (→ active ticket), v1.len == 0xd8 |
| 0x1c | DIVerify 0x20105bd0 | 4/2 | ctx->uid == 3; v2.len == 0x2a4, v3.len == TMD size, v4.len == 4, v5.len == ContentCount*0x14 |
| 0x1d | GetDataDirectory 0x201090f4 | 1/1 | v0.len == 8, v1.len == 0x1e |
| 0x1e | GetDeviceCert (syscall 0x70 directly) | 0/1 | v0.len == 0x180 |
| 0x1f | ImportBoot 0x20109740 | 6/0 | v0.len == 0x2a4 |
| 0x20 | GetTitleID (inline, active TMD TitleId) | 0/1 | v0.len == 8; -1017 if no active title |
| 0x21 | SetUID 0x20105790 | 1/0 | v0.len == 8 |
| 0x22 | DeleteTitleContents 0x2010857c | 1/0 | v0.len == 8 |
| 0x23 | SeekContent 0x20105494 | 3/0 | 3 × len 4 |
| 0x24 | OpenTitleContent 0x201051f0 | 3/0 | v0.len == 8, v1.len == 0xd8, v2.len == 4 |
| 0x25 | LaunchBC 0x20101288 | 0/0 | only if GetBusSpeed() != 0xa2 |
| 0x26 | ExportTitleInit 0x201061ec | 1/1 | v0.len == 8 |
| 0x27 | ExportContentStart 0x20106370 | 2/0 | v0.len == 8, v1.len == 4 |
| 0x28 | ExportContentData 0x20106448 | 1/1 | v0.len == 4 |
| 0x29 | ExportContentEnd 0x201064b8 | 1/0 | v0.len == 4 |
| 0x2a | ExportTitleDone 0x20106500 | 0/0 | – |
| 0x2b | ReimportTitleInit 0x20106068 | 1/0 | – |
| 0x2c | Encrypt 0x20106574 | 3/2 | v0.len == 4, v1.len == 0x10, v3.len == 0x10 (IV used), v4.len == v2.len |
| 0x2d | Decrypt 0x201065b0 | 3/2 | as 0x2c |
| 0x2e | GetBoot2Version (inline `IOSC_GetData(BOOT2_VERSION)`) | 0/1 | v0.len == 4 |
| 0x2f | AddTitleCancel 0x20104470 | 0/0 | – |
| 0x30 | Sign 0x20106dd4 | 1/2 | v1.len == 0x3c, v2.len == 0x180 |
| 0x31 | VerifySign 0x20106f08 | 3/0 | v1.len == 0x3c |
| 0x32 | GetStoredTMDContents (count) 0x20108280 | 1/1 | v0.len == TMD size, v1.len == 4 |
| 0x33 | GetStoredTMDContents | 2/1 | v2.len == n*4 |
| 0x34 | GetStoredTmd (size) 0x2010710c | 1/1 | v0.len == 8, v1.len == 4 |
| 0x35 | GetStoredTmd | 2/1 | v2.len == *v1 |
| 0x36 | GetSharedContents (count) 0x2010836c | 0/1 | v0.len == 4 |
| 0x37 | GetSharedContents | 1/1 | v1.len == n*0x14 |
| 0x38 | DeleteSharedContent 0x2010882c | 1/0 | v0.len == 0x14 (SHA-1) |
| 0x39 | GetActiveTitleTMD (size) 0x2010733c | 0/1 | v0.len == 4 |
| 0x3a | GetActiveTitleTMD | 1/1 | v1.len == *v0 |
| 0x3b | DiVerifyWithTicketView 0x20105f2c | 4/2 | ctx->uid == 3; v2.len == 0xd8, v3.len == TMD size, v4.len == 4, v5.len == ContentCount*0x14 |
| 0x3c | SetupStreamKey 0x201065ec | 2/1 | v0.len == 0xd8, v1.len == TMD size, v2.len == 4 |
| 0x3d | DeleteStreamKey 0x201068e8 | 1/0 | v0.len == 4 |
| 0x3e | DeleteTitleContent 0x20108660 | 2/0 | v0.len == 8, v1.len == 4 |
| 0x3f | – (not handled) | – | always -1017 |
| 0x40 | GetV0TicketFromView 0x20107974 | 1/1 | v0.len == 0xd8, v1.len == 0x2a4 |
| 0x41 | GetProcessPriorities 0x20109698 → syscall 0x79 | 1/1 | v0.len == 4 |
| 0x42 | SetProcessPriorities 0x20109678 → syscall 0x78 | 2/0 | v0.len == 4 |
| 0x43 | GetTicketFromView (size) 0x20107d30 | 1/1 | v0.len == 0xd8, v1.len == 4 |
| 0x44 | GetTicketFromView | 2/1 | v2.len == *v1 |
| 0x45 | HasKoreanKey 0x201096b8 | 0/0 | – |

---

## Appendix B — key layouts

**TMD view copy map** (`GenerateTMDView`, size `0x5c + 0x10·n`):
| View off | Source (TMD off) |
|---|---|
| +0x00 | Version (0x180) |
| +0x01..03 | zero (CRL/vWii not copied) |
| +0x04 | RequiredSystemVersion (0x184, u64) |
| +0x0c | TitleId (0x18c, u64) |
| +0x14 | TitleType |
| +0x18 | GroupID |
| +0x1a..57 | 0x3e bytes from TMD 0x19a..0x1d7 |
| +0x58 | TitleVersion |
| +0x5a | ContentCount |
| +0x5c+i·0x10 | `{u32 cid, u16 index, u16 type, u64 size}` |
Not copied: signature/issuer, AccessRights, BootIndex, MinorVersion, content SHA-1s.

**Startup directory table** (`CreateStartupDirectories`, as attr/owner/group/other):
`/sys` 0,3,3,0 · `/ticket` 0,3,3,0 · `/title` 0,3,3,**1** · `/shared1` 0,3,3,0 ·
`/shared2` 0,3,3,3 · `/tmp` 0,3,3,3 · `/import` 0,3,3,0 · `/meta` 0,3,3,3 · `/wfs` 0,3,**0,0**.
Then `SetAttr("/meta", uid(1-2), gid 1, …3,3,3)` and `SetAttr("/wfs", 0x13, 0x13, …3,0,0)`.

**Common keys**: `ES_CommonKeyHandles` (0x2010d004) = `{4, 0xB}` = OTP common key, EEPROM/Korean key.
**Shared content record**: `{char name[8]; u8 sha1[20]}` = 0x1c bytes. **cc.sys** keyed by ticket_id.

---

## Appendix C — IOS58 bugs to reproduce (bug-for-bug)

Each gets a source comment marking it a deliberate IOS58-compat reproduction:
- `GetTitles` list-mode off-by-one (writes `titles[N]`; returns 0 for one extra entry, `-1017` for two or more) — 0x20107f7a.
- `GetOwnedTitles` list-mode unbounded write (ignores `*count`) — 0x20107e70.
- `GetSharedContents` OOB map scan bounded by `.app` count — 0x201084cc.
- `ExportContentData` invalid-cfd path does `close(map[cfd].fd); map[cfd].fd=-1` unchecked — OOB
  write into the struct right after the 16×8 map (= `ES_CurrentActiveTitleContext`) — 0x2010648c.
- `ExportContentDataInner` rounds up to 32 B, writing up to 31 stale bytes past `data_size`.
- `AddContentFinish` closes the caller's fd on invalid args; `AddTitleCancel` fd leak.
- `HandleClose` leaks the context's TMD buffer + key object.
- `SetupStreamKey`/`DiVerifyInner` leak the key object on error paths.
- `LaunchTitleInternal` leaks its views buffer on success; `AddTitleStart` failure paths leak the context; `UpdateCertificateStore` leaks the fd on a post-open failure.
- `0x41`/`0x42` do not validate `v1.len` against `count*8` (overflow on Get).
- `ImportBoot` error paths call `IOSC_DeleteObject` with an uninitialised handle.
- `GetTitleUID` missing-uid.sys quirk (writes only 1-2, returns 0x1000 for any TID; new uid =
  last+1; first-match-wins); `CreateDirRecursive` re-chowns ancestors to 0:0; `ImportBoot` writes
  uninitialised heap padding; `GenerateTicketView` never sets `common_key_index`.
- `UpdateTicketLimits` clamps the timer index to 3 → reads one past the `u64[3]` tables.
- Concurrent imports across the 3 ESContexts collide on `/tmp/title.tmd` and `/tmp/<cid>.app`.

> If any reproduced bug turns out to crash StarStruck (vs. silently misbehave on IOS), stop and
> raise it — "bug-for-bug" assumes the behaviour is observable-but-survivable, as it is on IOS.

---

## Appendix D — decompiler artifacts (port from disassembly)

These decompile with wrong/missing arguments ("Heritage AFTER dead removal"); use the disasm:
- Dropped **path** string in: `CreateDirRecursive`, `GetTitleUID` (`/sys/uid.sys`, literal `0x2010c02c`),
  `CreateStartupDirectories` (SetAttr /meta,/wfs), `UpdateTicketLimits` (SetFileVerCtrl /tmp/cc.sys),
  `DIVerify` (SetAttr data dir), `UpdateCertificateStore` (/sys/cert.sys),
  `AccomodateNewTitle`/`RenameTitleImportContentDir`/`WriteTMDForNewTitle` (SetAttr paths),
  `DeleteSharedContent` (`0x20108b22` is `CreateAndWriteFile("/shared1/content.map", 0,3,3, 0, data, 0x1c*kept)`; the decompile shows `destination` as the path),
  the ImportV0/V1 (`CreateAndWriteFile` first arg is `ticketPath` at `sp+0x2c`) and DeleteV0/V1 (SetAttr path is the tmp path) ticket path args.
- `CreateDirRecursive` shows `in_stack_00000000`, which is really `other_perm` (5th arg). `UpdateCertificateStore` writes with `other=1`.
- `ES_SetProcessPriorities` 0x20109678 decompiles with an empty inner call but really passes `(r0=buf, r1=count)` to syscall 0x78 (the wrapper and the veneer share a name).
- Hidden **stack args**: `AddTitleStart` (8th = `*(u32*)vec3.data`), `LaunchTitle` (4th),
  `LaunchTitleInternal` (3rd u8), `ImportTicket` (2 stack params), the ES `IOS_ios_boot` veneer (3 args: path, suspend flag, version).
- `SeekContent` first arg is `ctx->uid` (0x20100528 `ldr r0,[r5,#4]`), not `vectors`.
- `GetDataDirectory` returns 0 (not `tid_hi`).
- `ES_main` disc.sys buffer typed `TitleMetadata*` → the IPC mask shows as `contents[5].ContentId+2`
  (really file offset 0x29a = `0xe0 + IpcMask 0x1ba`; `launch.sys` reads only `8 + 0xd8` bytes).

---

## Open RE questions

None block Phase 0 or Phase 1. Resolve before the dependent step:
1. FS module semantics: ioctl on a negative fd; recursive directory delete; ReadDir on a file entry; ReadDir ordering; rename of a directory onto an existing empty directory — before Phase 1b / 5 / 7.
2. `/dev/di` ioctl `0x8e` (`checkDVDPerms`) — before `TriggerActiveTitleAccessRights` (Phase 9).
3. Is the active-title TMD always set before the PPC can reach `0x24`/`0x3c`/`ChooseExportKeyForTitle`? — before Phase 6.
4. Purpose of the unreferenced code at `0x13a7a388`–`0x13a7a3ff` and of `ECDHParameters.Scalar` — before closing step E5.
5. Meaning of the registers `0x0d8b4200`/`0x0d8b4202` written by syscall `0x54` — before 0x54 (the writes are copied literally meanwhile).
6. Meaning of TitleType bits `0x04`/`0x20` in `SetupStreamKey` (the accepted combinations are known) — informational.
