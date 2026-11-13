# IOSC ECC (sect233r1) — IOS58 kernel reverse engineering

Ghidra program `IOS58_Kernel`, code at `0x13a7xxxx`. Status labels: **confirmed** = read from asm and/or reproduced by a literal
Python transcription; **emulated** = IOS's own code bytes (from Ghidra) run in an ARM big-endian emulator and checked against
the Python reference; **inferred** = read from structure, not run.

## Implementation steps E1-E6 (done; removed from `modules/es/ES_REIMPLEMENTATION_PLAN.md` on 2026-11-13)
The headings below that say E3, E4, E5, E6 are these steps of the ECC/ECDH work for syscall `0x61` (IOS58 addresses, `(size)` in bytes of IOS code):
| Step | File | What | Result recorded when it was done |
|---|---|---|---|
| E1 | `uint233.c` | `MultiplyField` (0x1f4): reduction by `x^233 + x^74 + 1`, `a == b` takes the square path | confirmed on 2506 cases; IOS's code in the emulator: 3632 pairs incl. 511 squares; the StarStruck port equals it on 5502 pairs (incl. 256 bit inputs and squares); stack 456 bytes, as in IOS |
| E2 | `uint233.c` | `InvertField` (0x4cc), `FinishInvertField` (0x1dc); ported faithfully (the Itoh-Tsujii alternative `a^(2^233-2)` was not used) | IOS's code in the emulator: `InvertField` on 3011 values (= the Fermat inverse, < 2^233, `a*b = x^bits` with bits 0..413 at the hand-over), `FinishInvertField` on bits 0..511; the port equals it on 7762 (`InvertField`, result and the `b`/`bits` hand-over) and 3512 (`FinishInvertField`, bits 0..699) values. IOS repeats the last step when the top words of u and v are equal; kept, it does not change the result |
| E3 | `curvePoint.c` | `CopyCurvePoint`, `PointDouble`, `PointAdd`, `PointSubtract` (~0x300) | `PointDouble` confirmed (2G, 4G on the curve); the port vs the group law of `gf233.py`: see the E3 section. Stack 200/192/88/16 (IOS 196/184/88) |
| E4 | `curvePoint.c` | `MultiplyPointByScalar` (0x12a), `MultiplyPointByScalarX` (0x24) | see the E4 section. Any correct scalar multiply gives identical results |
| E5 | `iosc_ecdh.c` | `ECDH_ComputeSharedSecretHash` (0x68), `HashEccSharedPoint` (0x6e) | see the E5 section; 39 OpenSSL / group-law vectors |
| E6 | `iosc.c` | `IOSC_ComputeSharedKey` (outer) and `_Inner`, table slot `0x61`, `OSIOSCComputeSharedKey` | see the E6 section; reviewed by the user, first end to end user is `DepersonalizeTicket` |

Call tree of syscall `0x61` (the byte <-> word helpers and word primitives are left out):
```
IOSC_ComputeSharedKey          0x13a72bf4  ownership checks, stack switch
 \ IOSC_ComputeSharedKey_Inner 0x13a70f18  keyring plumbing, type checks
    \ ECDH_ComputeSharedSecretHash 0x13a75218 (0x68)
       |- ECC_InitSect233r1Parameters  0x13a75768 (0xc66)
       |- ECC_MultiplyPointByScalarX   0x13a700e8 (0x24)   wrapper, returns x only
       |   \ ECC_MultiplyPointByScalar 0x13a76780 (0x12a)  signed-digit (NAF) double-and-add
       |      |- ECC_PointDouble 0x13a7662c (0xea)   |- ECC_PointAdd 0x13a76470 (0x1ac)   \ ECC_PointSubtract 0x13a76724 (0x54)
       |         field layer: ECC_InvertField 0x13a7a868 (0x4cc) -> ECC_FinishInvertField 0x13a7a68c (0x1dc)
       |                      ECC_MultiplyField 0x13a7a498 (0x1f4) -> MultiplyToWide / SquareToWide
       \ ECC_HashEccSharedPoint       0x13a750f8 (0x6e)   StoreUInt233 + SHA-1
```
About 31 functions and 5.2 KB of IOS code; `InvertField` (1.2 KB) is the largest, then `MultiplyToWide`, `MultiplyField`, `FinishInvertField` (~0.5 KB each).
IOS frames on the IOSC stack (push + `sub sp`): `ECDH` 188, `MultiplyPointByScalarX` 76, `MultiplyPointByScalar` 356, `PointAdd` 196, `PointDouble` 184, `PointSubtract` 88, `InvertField` 176, `FinishInvertField` 408, `MultiplyField` 456, `MultiplyToWide` 100, `SquareToWide` 76 (about 1.6 KB for the deepest chain). The StarStruck build (`make -C tests/ecc_test stack`, starts at `IOSC_ComputeSharedKey`): 2080 of the 4096 bytes of the IOSC stack, without the keyring, heap and ipc dispatch code.

Behaviour reproduced (bug for bug, from the decompile):
- Scalar (`LoadUInt233`): 30 bytes -> word 0 = `BE16 & 0x1ff`, words 1-7 = `BE32`; not reduced mod n. A zero scalar gives `(0,0)`, so x = 0 and the secret is SHA-1 of 30 zero bytes.
- Point (`LoadUInt233Pair`): 60 bytes = x (30) || y (30), loaded without masking. No on-curve check and no `(0,0)` check, so invalid points flow through the arithmetic.
- Output (`StoreUInt233`): only x, 30 bytes big-endian, written only through `memcpy` (byte stores to MEM1 trash the whole word and the core `memcpy` works around that). Secret = SHA-1 of those 30 bytes.
- Sizes: `outSize >= 0x1f` returns true and writes nothing. Otherwise `outSize` bytes are copied from a 32 byte local of which only the 20 SHA-1 bytes are written (decision: copy whatever is there, like IOS, no zero-fill). Callers pass 16 (AES-128).
- Scratch: IOS keeps `ECC_PointStorage`, the `MultiplyToWide` table (0x3c0) and the parameters (0x444) as globals, safe because IOSC calls are serialised by the IOSC mutex.
- `InvertField(0)` never returns in IOS and is reproduced; it is never reached: `PointDouble` returns early for `x == 0` and `PointAdd` for `x1 == x2`. `InvertField` of a value above 233 bits gives the inverse of `a mod f`.
- Host test byte order: `StoreUInt233` depends on Starlet being big-endian; on a little-endian host the tests swap each 4 byte group back and can only check the first 28 of the 30 bytes. The full 30 bytes are covered on Starlet (ECDH result).

## Curve / field
- GF(2^233), reduction poly f(x) = x^233 + x^74 + 1. Constant at `13a7af80` = `{0x200,0,0,0,0,0x400,0,1}` (confirmed).
- Curve y^2 + xy = x^3 + a*x^2 + b, a = 1, b/G from `ECC_InitSect233r1Parameters` (`13a75768`, was `ECC_InitParameters`); G is on the curve (confirmed).
- Affine coordinates, point at infinity = (0,0) (confirmed).
- Words are **big-endian word order**: word[0] = MSW (only 9 bits used, 233 = 7*32+9), word[7] = LSW. So `x[7] ^= 1` is "+1".

## Three data shapes (do not mix them up)
| Shape | Words | Layout | Used by |
|---|---|---|---|
| `UInt233` field element / scalar / coefficient | 8 (0x20) | BE, word 0 has 9 bits | `ECC_ClearUInt233`, `ECC_CopyUInt233`, params, scalars |
| `CurvePoint` | 16 (0x40) | x[8] @+0x00, y[8] @+0x20 | `ECC_CopyCurvePoint` (2x8), Point* functions |
| `uint[15]` (wide / unreduced product; StarStruck: `UInt233Wide` in `ellipticCurve/wideUint233.c/.h`) | **15** (0x3c) | BE, [14] = LSW; a UInt233 is zero-extended into words [7..14] (+0x1c). Plain `uint[15]`, no named type (max product length is 2*233-1 = 465 bits) | `ECC_Wide*`, `ECC_MultiplyToWide/SquareToWide`, `ECC_MultiplyField`, `ECC_FinishInvertField` only |
- 15 because 2*233-1 = 465 bits. `ECC_SquareToWide` builds a 16-word square but stores words 1..15 (word 0 is always 0). (confirmed)
- MultiplyToWide table: 16 entries, stride 0x3c (confirmed). Loop counters `0xe`/`0x7` are last-index (15 / 8 iterations).
- The `ECC_Wide*` helpers were originally typed `CurvePoint` / named `*_10_words`; that was wrong (they never touch a real point).
- Original source types are unknowable from the binary; only widths and strides are recoverable.

## Function map
| Addr | Name | Role | Status |
|---|---|---|---|
| 13a7662c | `ECC_PointDouble` | 2P: lambda=x+y/x; x3=lambda^2+lambda+a; y3=x^2+(lambda+1)x3. x==0 -> (0,0) | confirmed (2G, 4G on curve; equals textbook) |
| 13a76470 | `ECC_PointAdd` | P1+P2; P1==P2 -> Double; x1==x2,y differ -> (0,0) | read, not run |
| 13a76724 | `ECC_PointSubtract` | a-b = a + (b.x, b.x^b.y) | read |
| 13a76780 | `ECC_MultiplyPointByScalar` | signed-digit (digit = 2-(k&3) for odd k, +-1) double-and-add, MSB first | inferred |
| 13a768b8 | `ECC_PointCombTable16` | table[k] = (k0 + k1*2^64 + k2*2^128 + k3*2^192)*P, **not** k*P (scalar words 7,5,3,1 hold bits 0..3) | confirmed: the global `CurvePointCombTable` for G matches (all 16 entries) |
| 13a7690c | `FUN_13a7690c(UInt233* scalar, CurvePoint* table, CurvePoint* out, CurveParameters* parameters)` (unnamed) | 4-way comb scalar mult, 64 rounds of double+add, caller-supplied 16-entry table; 4-bit digits from bits at stride 64. Only caller: `FUN_13a70124` (= `ECC_EcScalarMultiply` look-alike using a prebuilt table, returns x only) | read |
| 13a76a1c | `ECC_MultiplyGenerator(UInt233* scalar, CurvePoint* generator, CurvePoint* out, CurveParameters* parameters)` | out = scalar * G with the global `CurvePointCombTable` (literal 13a76e90, not an argument): 64 rounds of out = 2*out + table[digit], digit r = bits p, p+64, p+128, p+192 of the scalar (p = 63 - r); scalar 0 -> (0,0). `generator` is unused; all callers pass G (`&GeneratorG` from FUN_13a70000, or `&ecdsaParameters->Generator` from FUN_13a70180 / ECDSA verify FUN_13a704dc). Returns nothing | read (2026-10-24), table confirmed |
| 13a76b38 / 13a76b80 | `FUN_13a76b38(CurvePoint* point, CurvePoint* table4, CurveParameters*)` / `FUN_13a76b80(UInt233* scalar, CurvePoint* point, CurvePoint* out, CurveParameters*)` (unnamed) | 2-way comb (bits 128 apart, 4-entry table); b80 caches last base point + table in static `DAT_13a76cf8` / `DAT_13a76d38`. **b80 has no xrefs at all (dead code, or only reached indirectly)**; b38 is only called by b80 | read |
| — | comb vs NAF | all scalar-mult variants compute the same k*P; comb is a speed optimisation (64 doublings instead of ~233). `ECC_MultiplyPointByScalarX` (was `ECC_EcScalarMultiply`) over the NAF `ECC_MultiplyPointByScalar` is the generic entry | math fact |
| 13a7ad3c | `ECC_HalveUInt233` | k = k / 2: the halving step of the NAF recoding (Guide to ECC alg. 3.30) in `MultiplyPointByScalar`, its only caller, which clears bit 0 first. The code is a rotate right by 1 within 233 bits (old bit0 -> bit 232 via carry-in 0x100; word0 &= 0x1ff after the shift), so odd input gives (k-1)/2 + 2^232 | confirmed (asm simulated vs rotate, 20000 values) |
| 13a7a498 | `ECC_MultiplyField` | GF mult; a==b -> square. Product*x^32 reduced via x^256=x^23+x^97 + 233-bit fold, then divide by x^32 | confirmed (2506 cases vs reference); emulated: 3632 pairs incl. 511 squares, all equal |
| 13a7a010 | `ECC_MultiplyToWide` | 4-bit windowed carry-less mult -> uint[15]. Uses GLOBAL scratch table `DAT_13a7b0b4..~13a7b474` (not reentrant) | confirmed |
| 13a7a400 | `ECC_SquareToWide` | bit-spread squaring, table `DAT_13a7ad80` (256 x u16 BE) | confirmed |
| 13a7a868 | `ECC_InvertField` | binary almost-inverse (u=a, v=f, b=1, c=0; `DAT_13a7afa0` = trailing-zero count per byte, [0]=8); b*a = x^bits | emulated (2026-10-23): 3011 values = the Fermat inverse, always < 2^233; at the call of FinishInvertField a*b = x^bits (mod f) with bits 0..413 (2004 values); a >= 2^233 gives the inverse of a mod f; **a = 0 loops forever** (no return in 5M instructions); stack 608 bytes incl. FinishInvertField |
| 13a7a68c | `ECC_FinishInvertField` | b * x^-bits mod f in 32-bit steps: m=v&mask; v=(v^(m<<233)^(m<<74))>>s | confirmed (contract + composed with textbook almost-inverse); emulated: bits 0..299, 466, 480, 500, 511 (1216 cases) |
| 13a79f8c / 13a79fcc | `ECC_WideLoad` / `ECC_WideStore` | zero-extend 8->15 words (words [7..14]) / truncate | confirmed |
| 13a79f18/30, 13a7a33c/68, 13a79f74 | `ECC_WideClear/Xor/And/ClearMask/Copy` | 15-word ops; `ClearMask` = bic (value &= ~mask); `Copy` is (src,dst) | confirmed |
| 13a7a21c / 13a7a2a8 / 13a79fe8 | `ECC_WideMultiply` / `Divide` / `Double` | in-place shifts of uint[15]: * 2^bits, / 2^bits (rounded down), * 2; as GF(2) polynomials * x^bits, drop the terms below x^bits, * x. `Double` is only used by MultiplyToWide for its b*x, b*x^2, b*x^3 table entries (0xe9=233, 0x4a=74 are f's exponents) | confirmed vs bigint |

## Globals written by ECC_InitSect233r1Parameters (split 2026-10-24)
The old 0x444-byte `ECDHParameters` global (`IOSC_ECC_Parameters`) was really three globals; no function reads past 0x44 of
`parameters`, and the table is only reached through its own hard-coded literal.
| Global | Addr | Type | Content |
|---|---|---|---|
| `CurveParameters` | 13a76e4c | `CurveParameters` (0x44) | `Scalar` = 1, a = 1, b |
| `CurvePointCombTable` | 13a76e90 | `CurvePoint[16]` | entry d = (d0 + d1*2^64 + d2*2^128 + d3*2^192) * G |
| `GeneratorG` | 13a77290 | `CurvePoint` | G |

`CurvePointCombTable` (was `UnknownCurvePoint`, `WorkingBasePointG?` and `CalculationsTable[14]`, then
`ECDHParameters.GeneratorCombTable`): d0..d3 are the bits of d, [0] = (0,0) (PointAdd treats it as +nothing), [1] = G,
[2] = 2^64 G, [4] = 2^128 G, [8] = 2^192 G, the rest sums. All 16 entries checked against the Python reference and OpenSSL
(`ec.derive_private_key(k, SECT233R1)`); all on the curve (confirmed). Used only by `ECC_MultiplyGenerator` (fixed-base comb,
64 doublings instead of ~233). Not needed for ECDH (it multiplies the peer's point).

## ECDSAParameters (FUN_13a70294(params, 0xe9), read 2026-10-24)
Built on the stack by the ECDSA wrappers, embeds the curve: `CurveParameters Curve` @0 (copy of the global), `CurvePoint Generator`
@0x44 (copy of GeneratorG), `uint Order[8]` @0x84 (group order n, mod-n word order: least significant word first), `uint Unknown[8]`
@0xa4 with word (0xe9>>5) = 2 (meaning not analysed). Point functions get `&parameters->Curve`, `ECC_MultiplyGenerator` also
`&parameters->Generator`.

## Who uses what (after the split, 2026-10-24)
| Function | Signature (Ghidra) | Uses |
|---|---|---|
| 13a70000 `FUN_13a70000` (private -> public) | `int (CurvePoint* generator, CurveParameters*, UInt233* scalar, CurvePoint* out)` | MultiplyGenerator, then checks y^2 + xy == x^3 + x^2 + b with `CoefficientB`; a = 1 is hard-coded (x^2 term), `CoefficientA` is not read. Returns 5 if out is (0,0) or off the curve, else 0 |
| 13a70114 `FUN_13a70114` | `int (CurvePoint* point, CurvePoint* table16, CurveParameters*)` | `ECC_PointCombTable16` wrapper |
| 13a70124 `FUN_13a70124` | `(CurvePoint* generator, CurveParameters*, CurvePoint* table, UInt233* scalar, UInt233* outX)` | FUN_13a7690c, generator unused |
| 13a70180 (ECDSA sign core) / 13a704dc (ECDSA verify core) | `ECDSAParameters*` | `Order`, `&Generator`, `&Curve` |
| 13a70294 | `(ECDSAParameters*, bits)` | fills it, calls `ECC_InitSect233r1Parameters` |
| 13a7031c | `ECDSAParameters*` | `&Curve` |
| 13a751b8 `IOSC_ECCPrivateToPublic_`, 13a75218 `ECDH_ComputeSharedSecretHash`, 13a7529c, 13a752fc | — | pass `&GeneratorG`, `&CurveParameters` |
| PointDouble / PointAdd / PointCombTable16 / MultiplyGenerator / 13a7690c / 13a76b38 / 13a76b80 | `CurveParameters*` | `Scalar` @0, `CoefficientA` @4 |
The tool-generated "Setting prototype:" headers on these still say `ECDHParameters` (stale, cannot be removed via the MCP).

## Struct layouts (read from Ghidra after the MCP update, 2026-10-05; split 2026-10-24)
| Type | Category | Layout |
|---|---|---|
| `UInt233` | `/ECDH` | typedef `uint[8]` (32 bytes) |
| `CurvePoint` | `/ECDH` | 0x40: `UInt233 x` @0x00, `UInt233 y` @0x20 |
| `CurveParameters` | `/ECDH` | 0x44: `ushort Scalar` @0x00, `ushort padding` @0x02, `UInt233 CoefficientA` @0x04, `UInt233 CoefficientB` @0x24 |
| `ECDSAParameters` | `/ECDH` | 0xc4: `CurveParameters Curve` @0, `CurvePoint Generator` @0x44, `uint Order[8]` @0x84, `uint Unknown[8]` @0xa4 |
| `ECCCert` | `/ES` | 0x180: `SignatureECDSA signature` @0, `CertificateHeader header` @0xc0, `byte key[60]` @0x108, `byte padding[60]` @0x144 |
These match what the asm showed (A coefficient at +4, flag at +0, y at +0x20); nothing needed correcting.
Other `CurvePoint`/`UInt233` accesses are exactly 8 words (UInt233) / 16 words (2 x 8 at +0x00/+0x20); exceptions are the 30-byte
packed<->8-word converters (`ECC_LoadUInt233`, `ECC_StoreUInt233`, `ECC_LoadUInt233Pair`, `ECC_StoreUInt233Pair`) and the comb digit arrays.

## Single number converters (confirmed from asm, 2026-10-21)
`ECC_LoadUInt233` (`13a720d0`, was `ECC_PrivateKeyToScalar`): 30 big-endian bytes -> `UInt233`, masked to 233 bits (word 0 `& 0x1ff`).
`ECC_StoreUInt233` (`13a72060`, was `ECC_EncodePoint32To30`): `UInt233` -> 30 big-endian bytes, the inverse. Not masked: the low 240 bits are
written (the value is shifted left 16 bits with mask `0x0000ffff` @`13a720c8`, then the first 30 bytes are copied). Neither is point-specific.

| Function | Call site | Caller | Value |
|---|---|---|---|
| Load | 13a75184 | `FUN_13a75178` (GenerateKey ECC path) | new random private key |
| Load | 13a751d6 | `IOSC_ECCPrivateToPublic_` | private key |
| Load | 13a75240 | `ECDH_ComputeSharedSecretHash` | private key (k of k·Q) |
| Load | 13a7531c | `FUN_13a752fc` (ECDH with comb table) | private key |
| Load | 13a753d4 / 13a753dc | `FUN_13a753a8` (ECDSA sign) | private key d / nonce k |
| Store | 13a75116 | `ECC_HashEccSharedPoint` | x of the shared point, into the global `ECC_PointStorage`, then SHA-1 |
| Store | 13a75196 | `FUN_13a75178` | the masked private key: Load + Store clears its top 7 bits before `IOSC_ECCPrivateToPublic_` |

`FUN_13a75178` is called from `FUN_13a70db8` (GenerateKey, 13a70ea8); its `memset(dst, 0, 4)` before the Store is redundant.
`ECC_HashEccSharedPoint`'s first parameter is named `point30` in Ghidra but is a 32-byte `UInt233` (x).
`13a7213c` (now `ECC_StoreBigEndianNumber` in Ghidra) is `StoreBigEndianNumber(u8* dst, s32 byteCount, const u32* srcWords, u32 wordCount)`, the inverse of
`LoadBigEndianNumber`; used by `ECC_StoreUInt233Pair` and the RSA code.

## Packed pair converters (confirmed from asm, 2026-10-21)
`ECC_LoadUInt233Pair` (`13a71fa8`) splits 0x3c big-endian bytes into two `UInt233` (`out[0]` = first 30 bytes, `out[1]` = last 30).
`ECC_StoreUInt233Pair` (`13a72004`) is the exact inverse. Neither is point-specific: `CurvePoint` is used as a container for two numbers.

| Call site | Caller | Output used as |
|---|---|---|
| 13a7524c | `ECDH_ComputeSharedSecretHash` (13a75218) | point Q -> `ECC_MultiplyPointByScalarX` (was `ECC_EcScalarMultiply`) |
| 13a752b0 | `FUN_13a7529c` | point -> `ECC_PointCombTable16`; the 16 entries are packed with `ECC_StoreUInt233Pair` into 16 x 0x3c bytes (comb table export) |
| 13a75332 | `FUN_13a752fc` | 16 calls fill `CurvePoint[16]` = comb table for `FUN_13a70124` (ECDH variant using the exported table) |
| 13a75452 | `FUN_13a75428` (ECDSA verify wrapper) | point Q -> `FUN_13a704dc` -> `ECC_MultiplyPointByScalar` |
| 13a7545c | `FUN_13a75428` | signature r ‖ s as `UInt233[2]` (stack arg `sig` at sp+0x4c); `FUN_13a704dc` reads +0x00 / +0x20 separately and reverses each back with `FUN_13a71f38` for mod-n math, never as a point |

`ECC_StoreUInt233Pair` callers: 13a751ee `IOSC_ECCPrivateToPublic_` (public key x ‖ y), 13a752d0 `FUN_13a7529c` (comb table export),
13a753fc `FUN_13a753a8` = ECDSA sign (signature r ‖ s).

## IOS source files (confirmed from call types, 2026-10-22)
IOS calls a function compiled earlier in the same source file with a direct `bl` (Ghidra `UNCONDITIONAL_CALL`) and every other
function with a long call (`ldr` + `blx`, `COMPUTED_CALL`), so direct calls mark file boundaries:
| File | Range | Evidence |
|---|---|---|
| F field | 13a79f00–13a7ad7f | `InvertField` → Clear/Copy, field code → `Wide*`/`MultiplyToWide` all `bl`; every outside caller long-calls |
| D points | 13a76450–13a76ce3 | `CopyCurvePoint`, `PointAdd`, scalar multiply called with `bl` inside; ECDSA verify long-calls them |
| B conversion | 13a71f38–13a721d9 | `Load/StoreUInt233Pair` → `ReverseWordOrder`/`13a71f38` with `bl`; RSA wrappers long-call `Load/StoreBigEndianNumber` |
| A EC ops / ECDSA cores | 13a70000–13a706f3 | long-calls D, E, F, B |
| E mod-n (RSAREF nn) | 13a772d0–13a77db3 | `13a77518` called with `bl` inside, long calls from A |
| C wrappers | 13a750f8–13a763cd | ECDH and `13a752fc` call `HashEccSharedPoint` with `bl`; `MultiplyPointByScalarX`/`LoadUInt233` long-called |
`ECC_CopyUInt233` is at `13a79f5c` (in F). `ECC_CopyCurvePoint` is at `13a76450` (in D). The plan's source layout follows this split.

## Gotchas for the reimplementation (`kernel/source/crypto/iosc_ecdh.c`)
- `ECC_PointDouble`/`PointAdd` write `dst`/`out` only if `parameters->Scalar != 0` (`ushort` @+0 followed by `ushort padding` @+2; =1 from `ECC_InitSect233r1Parameters`; **misnamed, real purpose unknown**. Speculative: a curve/field-type selector with only the binary-field path present in IOS58). `a` coefficient is at params+4.
- `ECC_PointDouble`: dst must not alias src (dst.x written before src.x is read for x^2).
- Any correct GF(2^233) arithmetic gives identical results; exact internal structure only matters if bit-exact timing/behaviour on invalid input matters (speculative).
- Types follow the original source's intent, not register widths: 0/1 flags in registers are `bool` (`differ` in `MultiplyField`,
  `InvertFieldIsOne`); carries, words and indices stay `u32` (2026-10-25).
- `ECC_MultiplyField` decompile shows a phantom `uint reversedProductMinus8[8]` (was `auStack_1e8`) in the x^256 fold loop:
  `reversedProductMinus8[i]` = `reversedProduct[i-8]`. The asm (13a7a524-13a7a568) addresses `reversedProduct[i-8]`, `[i-7]`, `[i-5]` as
  `r11-0x1c - 0x1c8/0x1c4/0x1bc + 4*i`; those constant bases lie below `reversedProduct`, so Ghidra binds them to an invented
  array. Only `[i-4]` (base -0x1b8) lands inside `reversedProduct`. i = 15..8 (`sub r12,#1; cmp r12,#7; bgt`), so every access
  is inside `reversedProduct` (word ranges 0..7, 1..8, 3..10, 4..11). Ghidra's own output gives it away: it indexes the 8-entry
  phantom with 8..18, i.e. past its end and into `reversedProduct`.
  Frame proof (confirmed, 2026-10-27): `stmdb` 8 regs + `sub sp,#0x1a8`; 0x1a8 = `reversedProduct[16]` + 6 x `uint[15]` exactly,
  no padding. r11-relative: reversedProduct -0x1c4 (= sp), reduced -0x184, m -0x148, folded -0x10c, mShl74 -0xd0, mShl233 -0x94,
  product -0x58, saved regs -0x1c. The phantom would be at sp-0x20, outside the frame: no hidden variable, `reversedProduct`
  is exactly 16 words. Ghidra cannot show negative indexes; the phantom was renamed and the plate comment explains it.
- Undefined code (no Ghidra function) at ~`13a7a388`-`13a7a3ff`: calls And/ClearMask/Divide/Xor helpers (probably another fold step). Not analysed.

## Test scaffolding (not in repo; was in the session scratchpad)
`ec_check.py` (GF + doubling vs sect233r1), `finalize_check.py` (FinishInvertField contract), `mul_check.py` (literal MultiplyField). Offer: add under `tools/` if wanted.

## Ghidra changes made (2026-10-05) — before -> after
Types: the wide buffers are plain `unsigned int[15]` (params `uint[15] *`, locals `uint x[15]`); no new named type is used.
A `UInt465` struct (root category, 60 bytes) was created first and then abandoned; it has since been deleted (gone per `get_struct`).
Existing `CurvePoint`/`UInt233`/`ECDHParameters` were NOT touched (they were unreadable via the MCP until its update on 2026-10-05).

| Addr | Before | After |
|---|---|---|
| 13a79f00 | `ECC_ClearUint223(UInt233*)` | `ECC_ClearUInt233` (typo) |
| 13a79f18 | `ECC_ClearCurvePoint(CurvePoint*)` | `ECC_WideClear(uint[15]*)` |
| 13a79f30 | `ECC_XorCurvePoint(CurvePoint* in,mask,out)` | `ECC_WideXor(uint[15]* a,b,out)` |
| 13a7a33c | `ECC_AndCurvePoints(CurvePoint*...)` | `ECC_WideAnd(uint[15]* a,b,out)` |
| 13a7a368 | `ECC_ClearMaskCurvePoint(CurvePoint* out,mask)` | `ECC_WideAndNot(uint[15]* value,mask)` |
| 13a7a368 | `ECC_WideAndNot` | `ECC_WideClearMask` (2026-10-23: named after what it does, clear the mask bits) |
| 13a79f74 | `copy_10_words(int* out,int* in)` (params swapped) | `ECC_WideCopy(uint[15]* src,dst)` |
| 13a7a21c | `windowed_shift_10_words(uint*,uint shift_amount)` | `ECC_WideShiftLeft(uint[15]*,uint bits)` |
| 13a7a2a8 | `FUN_13a7a2a8(int,uint)` | `ECC_WideShiftRight(uint[15]*,uint bits)` |
| 13a79fe8 | `FUN_13a79fe8(int)` | `ECC_WideShiftLeft1(uint[15]*)` |
| 13a7a21c / 13a7a2a8 / 13a79fe8 | `ECC_WideShiftLeft` / `ECC_WideShiftRight` / `ECC_WideShiftLeft1` | `ECC_WideMultiply` / `ECC_WideDivide` / `ECC_WideDouble` (2026-10-23: named after the math, * 2^bits, / 2^bits, * 2) |
| 13a79f8c | `ECC_ResetAndLoadYCoordinate(UInt233*,CurvePoint*)` | `ECC_WideLoad(UInt233* src,uint[15]* dst)` |
| 13a79fcc | `ECC_ExtractYCoordinate(CurvePoint*,UInt233*)` | `ECC_WideStore(uint[15]* src,UInt233* dst)` |
| 13a7a010 | `FUN_13a7a010(int,int,undefined4*)` | `ECC_ClMul(UInt233* a,b,uint[15]* out)` |
| 13a7a400 | `FUN_13a7a400(int,int)` | `ECC_ClSquare(UInt233* a,uint[15]* out)` |
| 13a7a010 / 13a7a400 | `ECC_ClMul` / `ECC_ClSquare` | `ECC_MultiplyToWide` / `ECC_SquareToWide` (2026-10-23: the unreduced product / square of two UInt233 into a UInt233Wide; ECC_FieldMultiply reduces it) |
| 13a7a498 | `FUN_13a7a498(UInt233*,UInt233*,UInt233*)` | `ECC_FieldMultiply(UInt233* a,b,out)` |
| 13a7a68c | `IOSC_FinalizeBigIntToField(int src,int bits,void* out)` | `ECC_FieldFinishInverse(UInt233* b,int bits,UInt233* out)` |
| 13a7a868 | `ECC_BigIntNormalizeShift(UInt233* coordinate,UInt233* tmp)` | `ECC_FieldInvert(UInt233* a,UInt233* out)` |
| 13a7a498 / 13a7a868 / 13a7a68c | `ECC_FieldMultiply` / `ECC_FieldInvert` / `ECC_FieldFinishInverse` | `ECC_MultiplyField` / `ECC_InvertField` / `ECC_FinishInvertField` (2026-10-24: verb first, like the StarStruck names) |
| 13a7662c | `ECC_CombineTransformA` | `ECC_PointDouble` |
| 13a76470 | `ECC_CombineAndProcess` | `ECC_PointAdd` |
| 13a76724 | `ECC_AdjustAndProcess` | `ECC_PointSubtract` (param c -> out) |
| 13a768b8 | `FUN_13a768b8(int,int,short*)` | `ECC_PointCombTable16(CurvePoint*,CurvePoint* table16,ECDHParameters*)` (first named `ECC_PointMultiplesTable16` with a wrong "k*P" comment, corrected; comment later updated to "confirmed") |
| 13a7ad3c | `ECC_RightShiftUInt233(UInt233* words)` | `ECC_RotateRightUInt233(UInt233* words)` (it is a rotate, see map above) |
| 13a7ad3c | `ECC_RotateRightUInt233(UInt233* words)` | `ECC_HalveUInt233(UInt233* words)` (2026-10-23: named after its mathematical action, k / 2) |

Also: buffer locals in `ECC_FinishInvertField` and `ECC_MultiplyField` set to `unsigned int[15]`; locals renamed
(`value,m,folded,mShl74,mShl233,mask` / `reduced,m,folded,mShl74,mShl233,product,productLE` / `b_u,v,c`);
decompiler/plate comments replaced on all functions above (old ones were wrong, e.g. "salt/ctx mixing").
Not changed: `ECC_MultiplyPointByScalar` (name/comment), `ECDHParameters.Scalar` field name, `CurvePoint`/`UInt233` definitions.

## Ghidra changes made (2026-10-27)
`ECC_MultiplyField` (13a7a498): locals `reversed` -> `reversedProduct`, `auStack_1e8` -> `reversedProductMinus8` (phantom,
see Gotchas); plate comment rewritten (the old one still said `productLE` / `auStack_1e8`). Same name as the C port.

## Ghidra changes made (2026-10-21) — before -> after
| Addr | Before | After |
|---|---|---|
| 13a71fa8 | `ECC_UnpackDualPoint` (briefly `ECC_PublicKeyToPoint`)`(void* compressedPoint,int pointSize,CurvePoint* out64)` | `void ECC_LoadUInt233Pair(byte* src,int srcSize,UInt233* out)`; locals -> `first`, `second` |
| 13a72004 | `FUN_13a72004(int,int,int)` | `void ECC_StoreUInt233Pair(UInt233* in,byte* dst,int dstSize)`; locals typed `UInt233 first`, `second` |
| 13a720d0 | `ECC_PrivateKeyToScalar` | `void ECC_LoadUInt233(byte* src,UInt233* out)` |
| 13a72060 | `ECC_EncodePoint32To30(void* out30,void* point32)` | `void ECC_StoreUInt233(byte* dst,UInt233* value)` |
Comments on both list the callers (Store's comment first said "points only", corrected: 13a753fc stores ECDSA r ‖ s). Both still carry the tool-generated "Setting prototype:" header.
Side effect: `FUN_13a75428`'s locals were re-inferred as `uint auStack_160[16]` (Q) and `uint auStack_120[65]` (sig + ECDHParameters merged).
Not yet retyped: Q `CurvePoint` @sp+0x0c, sig `UInt233[2]` @sp+0x4c (params @sp+0x8c re-inferred as `ECDSAParameters` on 2026-10-24).

## Ghidra MCP quirks
- `set_function_prototype` does **not** rename; use `rename_function`. It writes a tool-generated "Setting prototype:" header comment (goes stale after renames).
- Prototype strings can't resolve `uint`; use `unsigned int`.
- Many `set_function_prototype` calls issued in parallel can be silently lost (2 of 16 were) — re-read signatures to verify.
- `create_struct` with `size` + `members` stacks (60 + 60 = 120 bytes); `clear_struct` then `add_struct_members` gave the right size.
- Pointer-to-array params work as `unsigned int[15] *` (not `uint[15] *`, `uint` does not resolve); decompiles as `uint (*p)[15]`, like `UInt233 *`.
- Locals typed explicitly by earlier calls are not re-inferred when callee prototypes change; retype them with `set_local_variable_type` (`unsigned int[15]`).
- With the callee prototypes fixed, the mask stores (`mask[0xe] = 0xffffffff`) reappear in `ECC_FinishInvertField`.

## InvertField structure (read from asm, 2026-10-24)
Stack: v @fp-0xa8, c @-0x88, b @-0x68 (b[7] = 1), u @-0x48. r8 = `start` (first word that is non-zero in u or v, shared),
r6 = `top` (first used word of b and c, shared), r10 = bits. First u is made odd (b untouched). Then two symmetric steps:
u-step (u ^= v, b ^= c, then c <<= t / u >>= t until u is odd) and v-step (v ^= u, c ^= b, b <<= t / v >>= t). The larger one is chosen by
comparing only `u[start]` and `v[start]`; on equal words IOS repeats the step it did last (13a7a9d4 `bcc` vs 13a7aba0 `bcc`).
v == 1 copies c into b before the shared FinishInvertField call. At 13a7ab40 the u == 1 exit has an extra `index > 0` check that is always
true (dead fall-through). StarStruck port in `uint233.c` (E2).
Tie rule check (2026-10-24): about 2 ties per inversion, but a C build that breaks ties the other way gives identical results and
b/bits hand-over on 20000 values, so the rule is kept for fidelity only; the emulator cannot tell them apart.
C vs IOS differential (`scratchpad/diff/diff_c_ios.py`, C built as a shared lib): MultiplyField 5502 pairs, FinishInvertField 3512 cases,
InvertField 7762 values incl. the hand-over, 0 differences.

## Field file tables are from the source, not the compiler (2026-10-25)
- Compiler: `GCC: (GNU) 3.4.3` string at `13a7b0a1`, right after the field file's data (confirmed).
- `.rodata` order: squaring table `13a7ad80` (u16 bit spread), field polynomial `13a7af80`, trailing-zero table `13a7afa0`
  (`08 00 01 00 02 00 01 00 03 …`, 256 bytes) (confirmed).
- `InvertField` reads `DAT_13a7afa0` inline (`ldrb r12,[r9,r3]` at 13a7a8d0, 13a7aa48, 13a7ac14); there is no helper call (confirmed).
- No libgcc `__clz_tab` (`00 01 02 02 03 03 03 03 04 …`) in IOS58 (byte search, confirmed). A `__builtin_ctz` fallback would be a
  `bl __ctzsi2`; GCC 3.4 had no switch-to-table conversion (added in 4.4). So the table was written in the source (inferred), likely
  from the almost-inverse reference code (Schroeppel et al. 1995) (speculative).
- StarStruck: `TrailingZeroTable` is a literal 16x16 table in `uint233.c` (layout matches a hex dump). `__builtin_ctz((b & 0xFF) | 0x100)`
  is equivalent and on arm926ej-s compiles to `and/orr/rsb/and/clz/rsb` with no table (checked with devkitARM GCC 16.1); kept as a
  table for fidelity.

## Emulating IOS code (2026-10-23)
The field file (13a79f00-13a7b0a1, 4516 bytes incl. tables; all its internal calls are direct `bl`) can be run as is: dump it
with the Ghidra MCP `get_bytes`, map it at its address in Unicorn (`UC_ARCH_ARM`, `UC_MODE_ARM | UC_MODE_BIG_ENDIAN`, plus RW
pages for its bss up to 13a7b474, a stack and the arguments), set r0-r2, lr to a return address and run until lr. Used to
confirm `ECC_InvertField`, `ECC_FinishInvertField` and `ECC_MultiplyField` against `tests/ecc_test/ref/gf233.py`; a hook on
13a7ad24 (the `bl ECC_FinishInvertField` in InvertField) reads `b`/`bits` for the almost-inverse contract.

## StarStruck port of the group law, E3 (2026-11-12)
`kernel/source/crypto/ellipticCurve/curvePoint.c/.h`: `CopyCurvePoint`, `PointDouble`, `PointAdd`, `PointSubtract`, `CurveParameters` (0x44, same layout as Ghidra). Status: **implemented, to be verified** (not yet run against IOS's code in the emulator).
- Host tests (`tests/ecc_test/curvePoint_test.c`, vectors `ref/gen_curvePoint_vectors.py` from the `gf233.py` group law): 19 multiples of G + 20 random points + the x=0 point + infinity for double; ~250 add/subtract pairs (identity both sides, P+P, P+(-P), random). All equal (confirmed by test, 4469 checks total).
- Faithful to the decompile: infinity test of `PointAdd` is x|y of the point; `PointDouble` only tests x (so the x=0 point (0, sqrt b) doubles to (0,0)); the p1==p2 test compares x and y; x1==x2 with y differing gives (0,0).
- Deliberate difference: the `parameters->Scalar != 0` write guard is dropped (always 1). Aliasing constraints are kept: `PointDouble` dst != src, `PointAdd` out != p1 (out may equal p2: tested).
- Stack (ARM build, `make stack`): PointAdd 200, PointDouble 192, PointSubtract 88, CopyCurvePoint 16 (IOS 196/184/88): +4/+8 bytes, inside the budget (speculative: compiler differences).
- Next: E4 `MultiplyPointByScalar` (inferred, needs the test against OpenSSL) and the wrapper `MultiplyPointByScalarX` (was `EcScalarMultiply`).

## StarStruck port of the scalar multiply, E4 (2026-11-12)
`curvePoint.c`: `MultiplyPointByScalar(scalar, point, result, parameters)`, `ecc.c/.h`: `EcScalarMultiply(parameters, point, scalar, outX)` (IOS `0x13a700e8`, returns 0; keeps IOS's unused `generator` first argument, `(void)generator`). **Implemented, to be verified** against IOS's code; read from the Ghidra decompile (2026-11-12), structure now **confirmed by reading** (it was "inferred").
- Recoding: while k != 0: k odd -> digit = 1 if k&3==1, else -1 and k += 1 (carry runs up the words); even -> 0; then bit 0 cleared and `HalveUInt233`. Digits are lowest first, max 234 (IOS buffer 236). The top digit is always 1.
- Then result = P, and for every digit below the top: double; 0 -> take the double, 1 -> `PointAdd(point, doubled, result)`, -1 -> `PointSubtract(doubled, point, result)`. k = 0 -> result (0,0). The scalar is not reduced mod n; word 0 above 9 bits is lost in `HalveUInt233` (mask), only reachable with unmasked input (`LoadUInt233` masks).
- `result` must not alias `point` (point is read in every round).
- Tests (4769 checks total): k·P for G and a random point with 0, 1..7, 11, 15, n-1, n, n+1, 2^233-1, 2^232, runs of ones, 20 random; result equal to the group law of `gf233.py` (inverse in the generator is an extended Euclid, asserted `a*inv == 1`).
- The user re-added the `Scalar != 0` guard to `PointDouble`/`PointAdd` as an early return after the field math (2026-11-12); that replaces the "guard left out" note of the E3 section above.
- Next: E5 `ECDH_ComputeSharedSecretHash` + `HashEccSharedPoint` in `iosc_ecdh.c`, needs `ECC_InitSect233r1Parameters` only for a = 1 (see "Not needed for ECDH").

## Rename (2026-11-12)
| Addr | Old | New |
|---|---|---|
| 13a76780 | `ECC_EcScalarMultiplyPointInPlace` | `ECC_MultiplyPointByScalar` (StarStruck: `MultiplyPointByScalar`). It is not in place: `result` is a separate point and must not alias `point`. Older mentions of the old name in this file (and the 2026-10-21 "Not changed" note) now read as the new name |

## StarStruck port of ECDH, E5 (2026-11-13)
`kernel/source/crypto/iosc_ecdh.c`: `ECDH_ComputeSharedSecretHash` (IOS `0x13a75218`) and `static HashEccSharedPoint` (`0x13a750f8`). **Implemented, to be verified** (read from the Ghidra decompile, 2026-11-13).
- Flow (confirmed by reading): `outSize < 0x1f` -> `ECC_InitSect233r1Parameters`, `LoadUInt233(priv)`, `LoadUInt233Pair(pub, 0x3c)` into a `CurvePoint`, `EcScalarMultiply(&GeneratorG, &CurveParameters, &point, &scalar, &x)`, `HashEccSharedPoint(x, local)`, `memcpy(out, local, outSize)`; returns `outSize >= 0x1f`. The local is a `UInt233` (32 bytes), only the 20 digest bytes are written, so `outSize` 21..30 copies uninitialised bytes (callers pass 16).
- `HashEccSharedPoint`: `StoreUInt233(ECC_PointStorage, x)`, then `IOSC_GenerateHash_Inner` (= StarStruck `GenerateHash`) twice with the same context and `hashOutput[5]`, queue -1, no message: `(ctx, NULL, 0, InitShaState)`, `(ctx, ECC_PointStorage, 0x1e, FinalizeShaState)`; `memcpy(digestOut, hashOutput, 0x14)`. Return values are ignored. `GenerateHash` returns -2016 for an input that is not 0x40 aligned, so `ECC_PointStorage` (`0x13a754c0`, 0x40 aligned) has to be too: StarStruck uses a 0x40 byte `ALIGNED(0x40)` buffer in .bss (IOS keeps it in the crypto data; its real size is not known, speculative).
- `ECC_HashEccSharedPoint` frame (`13a750f8`, `sub sp,#0x80`, confirmed from the disassembly 2026-11-13): `sp+0x00..0x0b` outgoing stack args (digest, queue -1, message 0), `sp+0x0c` `hashOutput[5]` (0x14), `sp+0x20..0x7f` the context (0x60). Only `sp+0x20` is passed to `GenerateHash`, nothing else touches the rest. Ghidra's `ShaContext aSStack_80[3]` (0x54) is that one 0x60 local, not three contexts. 0x60 matches `IOSC_GenerateHash`'s `CheckMemoryPointer(context, 0x60, ...)`: the real hash state is probably 0x14 state + 8 length + 0x40 block buffer (inferred); the engine is only given `sizeof(ShaContext)` = 0x1c. StarStruck now uses `IOSCHashContext` (0x60) here too, same frame size as IOS (see below).
- Deliberate differences: the curve globals are `static const` instead of being rebuilt on every call (they are constant, `b` read from `ECC_InitSect233r1Parameters` and equal to the NIST B-233 `b`); G is not defined, `NULL` goes to the unused `generator`.
- Tests: `tests/ecc_test/iosc_ecdh_test.c`, vectors `ref/gen_iosc_ecdh_vectors.py` (was `gen_vectors.py`; 32 OpenSSL ECDH vectors, plus keys OpenSSL refuses (0, 1, n, n+1, 2^233-1, 240 and 234 bit keys) from the `gf233.py` group law). `GenerateHash` is replaced by a stub with its own SHA-1 that checks the init/finalize sequence, the 30 byte size and the 0x40 alignment. On a little-endian host the stub un-swaps `StoreUInt233`'s words and takes the last 2 bytes from the expected value (see the Store note in uint233_test.c). 6350 checks, 0 failures.
- Host build: `-idirafter sdk/include` gives the real `ios/sha.h` (the SDK's `string.h` would clash with the system one, hence `-idirafter`); `host/ios/ipc.h` replaces `<ios/ipc.h>`, whose layout asserts are 32-bit only.
- Stack (`make stack`, 2026-11-13): deepest counted chain `ECDH -> MultiplyPointByScalarX -> MultiplyPointByScalar -> PointSubtract -> PointAdd -> PointDouble -> MultiplyField -> MultiplyToWide -> WideLoad -> WideClear` = 1760 of 4096 bytes (IOS estimate ~1.6 KB). `GenerateHash` and the dispatch under it are **not counted** (other file, no .ci): IOS calls it on the same IOSC stack, so its depth still has to be checked.
- Next: E6, the glue in `iosc.c` (`IOSC_ComputeSharedKey` outer + `_Inner`, syscall `0x61`).

## Rename and move (2026-11-13)
| Addr | Old | New |
|---|---|---|
| 13a700e8 | `ECC_EcScalarMultiply(generator, parameters, point, scalar, outX)` | `ECC_MultiplyPointByScalarX` (StarStruck: `MultiplyPointByScalarX`). It is `ECC_MultiplyPointByScalar` plus a copy of the x coordinate and `return 0`; `generator` is never read. Not named `...Generator...`: `ECC_MultiplyGenerator` (13a76a1c) is the separate fixed-base comb `k*G`, and this one multiplies the caller's point (the peer's public key) |
- StarStruck moved it from the planned `ecc.c/.h` (IOS file A) into `curvePoint.c/.h` (IOS file D), next to `CurvePoint`, the type it takes. `ecc.c/.h`, `ecc_curve_test.c` and `gen_ecc_vectors.py` are gone; the test uses the `scalarVectors` of `curvePoint_test.c`. `ecc.c` is still the name for the rest of file A (`k*G` with on-curve check, ECDSA cores) when they arrive.
- Older mentions of `EcScalarMultiply` in this file (E4 section, 2026-11-13 ECDH section) now read as the new name.

## IOSCHashContext (2026-11-13)
- Ghidra (kernel+FS+ES in one program) already has `IOSCHashContext` (0x60, one member `sha: ShaContext` @0, 0x44 bytes undefined). Evidence for the size: the IOSC hash syscalls validate 0x60 bytes of the caller's context (`IOSC_GenerateHash`, `...Async`, `IOSC_GenerateBlockMAC`), `ES_AddContentStart/Data/Finish` keep it in `TitleImportExportContext.hashCtx` @0x20 and the next field (`currentContentHash`) starts at 0x80, and the kernel's own locals (`ECC_HashEccSharedPoint` frame, `FUN_13a71668`, `FUN_13a718d8`) reserve 0x60. Only `ShaStates`/`Length` (the first 0x1c bytes) are ever read or written, by the sha module; ES and the kernel callers only pass the pointer. What the other 0x44 bytes are for is unknown (confirmed: unused; purpose speculative).
- StarStruck: `IOSCHashContext { ShaContext ShaContext; u8 Unknown[0x44]; }` in `core/include/ios/iosc.h` (moved out of `sha.h`, which keeps `ShaContext`) (installed to `sdk/include` by `make -C core`, which the modules and the host tests read). The public API takes it: `OSIOSCGenerateHash(Async)`, `OSIOSCGenerateBlockMAC(Async)` (syscalls.h), `IOSC_GenerateHash(Async)`, `IOSC_GenerateBlockMAC(Async)` (they now check `sizeof(IOSCHashContext)` instead of a literal 0x60), `GenerateHash`/`GenerateBlockMAC` (iosc_hash). The sha module/engine (`sha.c`) keeps `ShaContext`, and `GenerateHash`/`GenerateBlockMAC` still send `sizeof(ShaContext)` (0x1c) to it. Callers changed: `HashEccSharedPoint`, the HMAC local in `iosc.c` and the two in `modules/fs/.../cluster.c`.
- Stack: `make stack` deepest chain 1760 -> 1832 bytes of 4096 with the 0x60 context in `HashEccSharedPoint`.
- Ghidra: the kernel locals typed `ShaContext aSStack[3]` could be retyped to `IOSCHashContext` (not done).
- Alignment (2026-11-13, confirmed from the disassembly): IOS does not align its hash context or digest locals. `IOSC_ImportKey_Inner` (`13a70850`, `sub sp,#0x9c`: context `sp+0x24`, digest `sp+0x84`), `FS_WriteClustersInner_` (`20004b34`, `sub sp,#0x134`: context `sp+0x34`, digest `sp+0xb4`) and `FS_ReadClustersInner_` (`20004dd0`, `sub sp,#0x110`: context `sp+0x3c`, digest `sp+0xfc`) have no stack realignment, and `GenerateHash`/`GenerateBlockMAC` only check the alignment of `inputData`. StarStruck dropped `aligned(0x40)` from `hmacContext`/`computedHmac` (`iosc.c`) and `hmacContext`/`hmacDigest` (`cluster.c`, 2 functions). The `= { 0 }` initialisers are not in IOS either (no memset of the context), left as they were.
- Possible difference, not changed: `IOSC_ImportKey_Inner` passes `signBuffer` as the signData argument of the HMAC init call (`IOSC_GenerateBlockMACInner(ctx, 0, 0, 0, 0, verifyHandle, init, signBuffer, -1, 0)`), StarStruck's `iosc.c:586` passes `NULL` there. The FS code passes its digest buffer in the init call like IOS does.

## ECDH curve init (2026-11-13)
- `iosc_ecdh.c`: `static InitSect233r1Parameters()` (IOS `ECC_InitSect233r1Parameters`, `0x13a75768`) fills the globals `curveParameters` (IOS `CurveParameters` `0x13a76e4c`; lower case because `CurveParameters` is the type in C) and `generatorG` (`GeneratorG` `0x13a77290`): `Scalar` = 1, a = 1, b, G; read from the Ghidra decompile, equal to the NIST B-233 constants of `gf233.py` (tested against the generator's values). `ECDH_ComputeSharedSecretHash` calls it after the `outSize` check, like IOS, and passes `&generatorG` and `&curveParameters` to `MultiplyPointByScalarX`. The 16 entry `CurvePointCombTable` (`0x13a76e90`) is filled in the same function too (`_curvePointCombTable`, 16 x 0x40 bytes, only the generator multiply will read it). The entries were generated from the group law (entry d = (d0 + d1*2^64 + d2*2^128 + d3*2^192) * G, `gf233.py`), all 16 on the curve, and 5 of them (2, 4, 9, 0xb, 0xf) compared word for word with the Ghidra decompile (2026-11-13); the test compares all 16 against the generator's values. The globals were renamed `_curveParameters` / `_generatorG` by the user.
- ECDH return value: IOS returns `outSize >= 0x1f` (true = refused, nothing written); a 2026-11-13 edit returned the inverse, which the tests flagged; the tests pass again after the follow-up edit.

## Copy audit (2026-11-13)
- Every `UInt233` / `UInt233Wide` / `CurvePoint` / `CurveParameters` parameter in `ellipticCurve/*.c` and `iosc_ecdh.c` is a pointer: the first two are array typedefs (can't be passed by value in C), the structs are all declared `*`. No function returns one by value.
- The copies inside the functions were compared with the Ghidra decompiles and are all in IOS too: `MultiplyField` (`WideLoad(out, reduced)`, the `m` -> `mShl233`/`mShl74` loop), `FinishInvertField` (`WideLoad(b, value)`, the copy loop, `WideCopy(m, ...)`, `WideCopy(mShl233, value)`), `MultiplyToWide` (b -> table[1] -> `multiple` -> table[2/4/8]), `SquareToWide` (16-word `square`, words 1..15 out), `InvertField` (`a` -> u, f -> v, `CopyUInt233(c, b)` before the finish), `LoadUInt233Pair` (two locals + `ReverseWordOrder`), `StoreUInt233` (word buffer + memcpy), `PointSubtract` (local -b), `PointAdd` (identity copies), `MultiplyPointByScalar` (scalar -> k, point -> result, doubled -> result), `MultiplyPointByScalarX` (local point, copy of x), ECDH (locals `scalar`, `point`, x, digest; digest memcpy). No extra copies were found.
- Only StarStruck-side differences in this area: `InitSect233r1Parameters` assigns compound literals (the compiler may copy them from read-only data instead of IOS's immediate stores), and the `= { 0 }` / `ClearUInt233` choices already noted above.

## ComputeSharedKey syscall 0x61, E6 (2026-11-13)
- Read from the Ghidra decompile and the disassembly (`IOSC_ComputeSharedKey` `13a72bf4`, `_Inner` `13a70f18`). **Implemented, to be verified**: `IOSC_ComputeSharedKey` + `static _IOSC_ComputeSharedKeyInner` in `iosc.c`, table slot `0x0061` in `interrupt/syscall.cpp`, user stub `OSIOSCComputeSharedKey` (`core/source/ios/syscalls_asm.s`, `core/include/ios/syscalls.h`). No host test: `iosc.c` needs the keyring, heaps and the thread state; the first end to end user is `DepersonalizeTicket` (ES).
- Outer (IOSC safety wrapper, same as the other syscalls): discard queue message, switch to the IOSC stack, the caller's process must own the private, public and shared key (the root key `0x0FFFFFFF` skips the check, `IOSC_CheckCurrentProcessOwnsKey` does that), any failure gives -2000 (`IOSC_EACCES`), then the inner function.
- Inner (confirmed from the disassembly): shared handle must be > 11 and not the root key (else -2000). Zeroes (`Keyring_Get/SetKeyZeroesIfAnyPrivate`) are copied from the private to the shared key, errors are returned as is. Private key: type Private or PublicAndPrivate, subtype ECC_233 (else -0x7d5 = `IOSC_INVALID_OBJTYPE`); public key: type Public or PublicAndPrivate, subtype ECC_233 (-0x7d5); shared key: type Private and subtype AES_128 or HMAC (< RSA_2048) (-0x7d5). Then `Keyring_GetKey(private, 0x1e bytes)`, `Keyring_FindKeySize(public)`; a Public key is read as is (0x3c bytes), a PublicAndPrivate key is read (0x5a = 90 bytes, confirmed against IOS's `Keyring_GetKeySizeFromType` `13a71d30`: ECC public 0x3c, private 0x1e, public+private 0x5a) into one stack buffer at `sp+0x10`; the public part is copied from `sp+0x2e` (= buffer + 0x1e). Ghidra shows two variables, `auStack_f0[30]` and `auStack_d2[62]`, because it sizes a variable as the gap to the next one it knows of (the read target and the copy source); the 62 is 60 bytes plus 2 bytes of padding up to the next 4-byte aligned buffer at `sp+0x6c` (inferred). StarStruck: one `u8[90]` and `&combinedKey[0x1e]` (the memcpy happens before the error check; other types give -0x7d8 = `IOSC_INVALID_FORMAT`, unreachable). `Keyring_FindKeySize(shared)`, `ECDH_ComputeSharedSecretHash(private, public, sharedBuf, sharedSize)`: **true = error** (size >= 0x1f), -0x7dc (`IOSC_FAIL_INTERNAL`) as for every other keyring failure, then the size of the shared key is asked again (into the `publicKeySize` variable) and `Keyring_SetKey(shared, sharedBuf, size)`.
- Stack frames in the decompile: inner `sub sp,#0xe8`: key type/subtype at `sp+0xf/0xe`, shared size `sp+0`, public size `sp+4`, zeroes `sp+8`, combined key `sp+0x10` (90 bytes + 2 padding), private key `sp+0x8c`, public key `sp+0xac`, shared key buffer `sp+0x6c` (0x20).
- Side observation: `IOSC_ImportSecretKey` in `iosc.c` skips the ownership check for `(u32)-1`, the IOS decompile compares with `0x0FFFFFFF` (the root key). Not changed.
- Link: the three `UInt233` helpers in `uint233.h` have to be `static inline`; plain `inline` gives undefined references as soon as the code is really linked (the syscall is the first user of the ECC code in the kernel, before it the linker dropped it).
- `make stack` now starts at `IOSC_ComputeSharedKey` (`iosc.c`, `iosc_hash.c` added): deepest counted chain 2104 of 4096 bytes. Not counted (other files): keyring functions, `DispatchIoctlv`, `AllocateOnHeap`/`FreeOnHeap`, message queue calls, `IOSC_SwapStack`.

## ECC key types (2026-11-13)
- `kernel/source/crypto/iosc_ecdh.h` (first in a separate `ellipticCurve/eccKey.h`, moved because only the kernel crypto code uses them; if a module like ES needs them later they belong in `core/include/ios/` with literal sizes, `UINT233_NUM_BYTES` is a kernel header): `ECCPrivateKey` (`u8[UINT233_NUM_BYTES]`, 0x1e), `ECCPublicKey` (`{ u8 X[30]; u8 Y[30]; }`, 0x3c), `ECCPublicAndPrivateKey` (`{ ECCPrivateKey Private; ECCPublicKey Public; }`, 0x5a, `Public` at 0x1e). All `u8`, so no padding; they are the keyring format (big-endian bytes), not the `UInt233` word form (that would be 32 bytes per number).
- Used by: the keyring size table (`Keyring_GetKeySizeFromType`, ECC public / private / both), `ECDH_ComputeSharedSecretHash(const ECCPrivateKey, const ECCPublicKey*, ...)` (the `ECDH_PRIVATE_KEY_SIZE` / `ECDH_PUBLIC_KEY_SIZE` macros are gone), and `_IOSC_ComputeSharedKeyInner` (combined key buffer, `memcpy(&publicKey, &combinedKey.Public, sizeof(ECCPublicKey))`).
- `ECCCert` in Ghidra has `key[60]`.
- Ghidra (2026-11-13): structs `ECCPublicKey` (0x3c: `X` byte[30] @0, `Y` byte[30] @0x1e) and `ECCPublicAndPrivateKey` (0x5a: `Private` byte[30] @0, `Public` ECCPublicKey @0x1e) created (no typedef tool, so `ECCPrivateKey` is `byte[30]` as the struct member and the 32 byte `privateKey` local stays `byte[32]`). In `IOSC_ComputeSharedKey_Inner` (`13a70f18`) the locals `auStack_f0` + `auStack_d2` are now one `ECCPublicAndPrivateKey combinedKey` (Ghidra absorbed the 62 byte variable; `memcpy(&publicKey, &combinedKey.Public, 0x3c)`), `publicKey` is an `ECCPublicKey`, `abStack_94` is renamed `sharedKey`. `ECDH_ComputeSharedSecretHash` (`13a75218`) prototype: `bool (byte* privateKey, ECCPublicKey* publicKey, byte* out, uint outSize)`.
- Confirmed 2026-11-13: IOS `Keyring_GetKeySizeFromType` (`13a71d30`) ECC sizes are public 0x3c, private 0x1e, public+private 0x5a; same as StarStruck's `keyring.c`.

## HMAC init sign data and Keyring_GetSignatureSize (2026-11-13)
- `IOSC_GenerateBlockMACInner` (IOS `13a73bd0`, StarStruck `GenerateBlockMAC`) puts the sign data in vector 2 with length 0x14, 0 when it is NULL. `IOSC_ImportKey_Inner` (`13a70850`) passes `signBuffer` there in the HMAC **init** call (and the digest buffer in the finalize call); StarStruck passed `NULL`, now `signBuffer` like IOS (`iosc.c`). The SHA module's HMAC init handler (`sha.c`) only reads vectors 1 (context), 3 (key handle) and 4 (custom data), so the engine result does not depend on it; the vector only matters to the IPC dispatch (confirmed by reading, the dispatch code was not read).
- `Keyring_GetSignatureSize` (IOS `13a712f0`): the root key (`0x0FFFFFFF`) returns 0x200 before anything else (RSA-4096 signer): StarStruck lacked that, added. Other cases were already the same: public RSA4096 0x200, RSA2048 0x100, ECC 0x3c; private HMAC 0x14, ECC 0x3c; both ECC 0x3c; anything else -0x7d2 (`IOSC_EINVAL`). The sizes are now `sizeof(ECCSignature)` (new type next to the key types in `iosc_ecdh.h`: `{ u8 R[30]; u8 S[30]; }`, 0x3c) and `sizeof(FinalShaHash)`; parameter renamed `signatureSize`. Ghidra: struct `ECCSignature` (0x3c) created.

## Status (2026-11-13)
Syscall `0x61` `IOSC_ComputeSharedKey` marked DONE in `modules/es/ES_REIMPLEMENTATION_PLAN.md` after review by the user. Not yet exercised end to end (first real user: ES `DepersonalizeTicket`); E3-E5 rows in the plan still say "to be verified" (emulator comparison against IOS's code).
