# IOSC ECC (sect233r1) — IOS58 kernel reverse engineering

Ghidra program `IOS58_Kernel`, code at `0x13a7xxxx`. Status labels: **confirmed** = read from asm and/or reproduced by a literal
Python transcription; **inferred** = read from structure, not run.

## Curve / field
- GF(2^233), reduction poly f(x) = x^233 + x^74 + 1. Constant at `13a7af80` = `{0x200,0,0,0,0,0x400,0,1}` (confirmed).
- Curve y^2 + xy = x^3 + a*x^2 + b, a = 1, b/G from `ECC_InitParameters` (`13a75768`); G is on the curve (confirmed).
- Affine coordinates, point at infinity = (0,0) (confirmed).
- Words are **big-endian word order**: word[0] = MSW (only 9 bits used, 233 = 7*32+9), word[7] = LSW. So `x[7] ^= 1` is "+1".

## Three data shapes (do not mix them up)
| Shape | Words | Layout | Used by |
|---|---|---|---|
| `UInt233` field element / scalar / coefficient | 8 (0x20) | BE, word 0 has 9 bits | `ECC_ClearUInt233`, `ECC_CopyUInt233`, params, scalars |
| `CurvePoint` | 16 (0x40) | x[8] @+0x00, y[8] @+0x20 | `ECC_CopyCurvePoint` (2x8), Point* functions |
| `uint[15]` (wide / unreduced product; StarStruck: `UInt233Wide` in `ellipticCurve/wideUint233.c/.h`) | **15** (0x3c) | BE, [14] = LSW; a UInt233 is zero-extended into words [7..14] (+0x1c). Plain `uint[15]`, no named type (max product length is 2*233-1 = 465 bits) | `ECC_Wide*`, `ECC_MultiplyToWide/SquareToWide`, `ECC_FieldMultiply`, `ECC_FieldFinishInverse` only |
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
| 13a76780 | `ECC_EcScalarMultiplyPointInPlace` | signed-digit (digit = 2-(k&3) for odd k, +-1) double-and-add, MSB first | inferred |
| 13a768b8 | `ECC_PointCombTable16` | table[k] = (k0 + k1*2^64 + k2*2^128 + k3*2^192)*P, **not** k*P (scalar words 7,5,3,1 hold bits 0..3) | confirmed: ECDHParameters' built-in table for G matches (k=2,3,4,8,15 of 14 checked) |
| 13a7690c | `FUN_13a7690c` (unnamed) | 4-way comb scalar mult, 64 rounds of double+add, caller-supplied 16-entry table; 4-bit digits from bits at stride 64. Only caller: `FUN_13a70124` (= `ECC_EcScalarMultiply` look-alike using a prebuilt table, returns x only) | read |
| 13a76a1c | `FUN_13a76a1c` (unnamed) | same, but uses the generator table inside ECDHParameters (`&UnknownCurvePoint + idx`, 2nd param unused). Callers: `FUN_13a70000` (k*G, then checks result != (0,0) and y^2+xy = x^3+x^2+b, `a` hard-coded as 1; returns 5 on failure), `FUN_13a70180` (big-int mod arithmetic then k*G; not analysed), `FUN_13a704dc` (not analysed) | read |
| 13a76b38 / 13a76b80 | `FUN_13a76b38` / `FUN_13a76b80` (unnamed) | 2-way comb (bits 128 apart, 4-entry table); b80 caches last base point + table in static `DAT_13a76cf8` / `DAT_13a76d38`. **b80 has no xrefs at all (dead code, or only reached indirectly)**; b38 is only called by b80 | read |
| — | comb vs NAF | all scalar-mult variants compute the same k*P; comb is a speed optimisation (64 doublings instead of ~233). `ECC_EcScalarMultiply` (NAF) is the generic entry | math fact |
| 13a7ad3c | `ECC_HalveUInt233` | k = k / 2: the halving step of the NAF recoding (Guide to ECC alg. 3.30) in `EcScalarMultiplyPointInPlace`, its only caller, which clears bit 0 first. The code is a rotate right by 1 within 233 bits (old bit0 -> bit 232 via carry-in 0x100; word0 &= 0x1ff after the shift), so odd input gives (k-1)/2 + 2^232 | confirmed (asm simulated vs rotate, 20000 values) |
| 13a7a498 | `ECC_FieldMultiply` | GF mult; a==b -> square. Product*x^32 reduced via x^256=x^23+x^97 + 233-bit fold, then divide by x^32 | confirmed (2506 cases vs reference) |
| 13a7a010 | `ECC_MultiplyToWide` | 4-bit windowed carry-less mult -> uint[15]. Uses GLOBAL scratch table `DAT_13a7b0b4..~13a7b474` (not reentrant) | confirmed |
| 13a7a400 | `ECC_SquareToWide` | bit-spread squaring, table `DAT_13a7ad80` (256 x u16 BE) | confirmed |
| 13a7a868 | `ECC_FieldInvert` | binary almost-inverse (u=a, v=f, b=1, c=0; `DAT_13a7afa0` = trailing-zero count per byte, [0]=8); b*a = x^bits | inferred (not instruction-tested) |
| 13a7a68c | `ECC_FieldFinishInverse` | b * x^-bits mod f in 32-bit steps: m=v&mask; v=(v^(m<<233)^(m<<74))>>s | confirmed (contract + composed with textbook almost-inverse) |
| 13a79f8c / 13a79fcc | `ECC_WideLoad` / `ECC_WideStore` | zero-extend 8->15 words (words [7..14]) / truncate | confirmed |
| 13a79f18/30, 13a7a33c/68, 13a79f74 | `ECC_WideClear/Xor/And/ClearMask/Copy` | 15-word ops; `ClearMask` = bic (value &= ~mask); `Copy` is (src,dst) | confirmed |
| 13a7a21c / 13a7a2a8 / 13a79fe8 | `ECC_WideMultiply` / `Divide` / `Double` | in-place shifts of uint[15]: * 2^bits, / 2^bits (rounded down), * 2; as GF(2) polynomials * x^bits, drop the terms below x^bits, * x. `Double` is only used by MultiplyToWide for its b*x, b*x^2, b*x^3 table entries (0xe9=233, 0x4a=74 are f's exponents) | confirmed vs bigint |

## ECDHParameters generator table (confirmed)
`UnknownCurvePoint` (= entry 0, (0,0)), `WorkingBasePointG_` (= entry 1, G) and `CalculationsTable[0..13]` (= entries 2..15) are ONE contiguous
16-entry comb table for G (`FUN_13a76a1c` indexes `&UnknownCurvePoint + idx`). Entry k = (k0 + k1*2^64 + k2*2^128 + k3*2^192)*G.
Struct offsets confirm the contiguity: `UnknownCurvePoint` @0x44, `WorkingBasePointG?` @0x84, `CalculationsTable[14]` @0xc4 = 16 consecutive CurvePoints.
Not renamed. Suggestion: one `CurvePoint GeneratorCombTable[16]` @0x44 (entry 1 is G, so the `?` in `WorkingBasePointG?` can go).

## Struct layouts (read from Ghidra after the MCP update, 2026-10-05)
| Type | Category | Layout |
|---|---|---|
| `UInt233` | `/ECDH` | typedef `uint[8]` (32 bytes) |
| `CurvePoint` | `/ECDH` | 0x40: `UInt233 x` @0x00, `UInt233 y` @0x20 |
| `ECDHParameters` | `/ECDH` | 0x444: `ushort Scalar` @0x00, `ushort padding` @0x02, `UInt233 CurveACoefficient` @0x04, `UInt233 CurveBCoefficient` @0x24, `CurvePoint UnknownCurvePoint` @0x44, `CurvePoint WorkingBasePointG?` @0x84, `CurvePoint CalculationsTable[14]` @0xc4 |
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
| 13a7524c | `ECDH_ComputeSharedSecretHash` (13a75218) | point Q -> `ECC_EcScalarMultiply` |
| 13a752b0 | `FUN_13a7529c` | point -> `ECC_PointCombTable16`; the 16 entries are packed with `ECC_StoreUInt233Pair` into 16 x 0x3c bytes (comb table export) |
| 13a75332 | `FUN_13a752fc` | 16 calls fill `CurvePoint[16]` = comb table for `FUN_13a70124` (ECDH variant using the exported table) |
| 13a75452 | `FUN_13a75428` (ECDSA verify wrapper) | point Q -> `FUN_13a704dc` -> `ECC_EcScalarMultiplyPointInPlace` |
| 13a7545c | `FUN_13a75428` | signature r ‖ s as `UInt233[2]` (stack arg `sig` at sp+0x4c); `FUN_13a704dc` reads +0x00 / +0x20 separately and reverses each back with `FUN_13a71f38` for mod-n math, never as a point |

`ECC_StoreUInt233Pair` callers: 13a751ee `IOSC_ECCPrivateToPublic_` (public key x ‖ y), 13a752d0 `FUN_13a7529c` (comb table export),
13a753fc `FUN_13a753a8` = ECDSA sign (signature r ‖ s).

## IOS source files (confirmed from call types, 2026-10-22)
IOS calls a function compiled earlier in the same source file with a direct `bl` (Ghidra `UNCONDITIONAL_CALL`) and every other
function with a long call (`ldr` + `blx`, `COMPUTED_CALL`), so direct calls mark file boundaries:
| File | Range | Evidence |
|---|---|---|
| F field | 13a79f00–13a7ad7f | `FieldInvert` → Clear/Copy, field code → `Wide*`/`MultiplyToWide` all `bl`; every outside caller long-calls |
| D points | 13a76450–13a76ce3 | `CopyCurvePoint`, `PointAdd`, scalar multiply called with `bl` inside; ECDSA verify long-calls them |
| B conversion | 13a71f38–13a721d9 | `Load/StoreUInt233Pair` → `ReverseWordOrder`/`13a71f38` with `bl`; RSA wrappers long-call `Load/StoreBigEndianNumber` |
| A EC ops / ECDSA cores | 13a70000–13a706f3 | long-calls D, E, F, B |
| E mod-n (RSAREF nn) | 13a772d0–13a77db3 | `13a77518` called with `bl` inside, long calls from A |
| C wrappers | 13a750f8–13a763cd | ECDH and `13a752fc` call `HashEccSharedPoint` with `bl`; `EcScalarMultiply`/`LoadUInt233` long-called |
`ECC_CopyUInt233` is at `13a79f5c` (in F). `ECC_CopyCurvePoint` is at `13a76450` (in D). The plan's source layout follows this split.

## Gotchas for the reimplementation (`kernel/source/crypto/iosc_ecdh.c`)
- `ECC_PointDouble`/`PointAdd` write `dst`/`out` only if `parameters->Scalar != 0` (`ushort` @+0 followed by `ushort padding` @+2; =1 from `ECC_InitParameters`; **misnamed, real purpose unknown**. Speculative: a curve/field-type selector with only the binary-field path present in IOS58). `a` coefficient is at params+4.
- `ECC_PointDouble`: dst must not alias src (dst.x written before src.x is read for x^2).
- Any correct GF(2^233) arithmetic gives identical results; exact internal structure only matters if bit-exact timing/behaviour on invalid input matters (speculative).
- Undefined code (no Ghidra function) at ~`13a7a388`-`13a7a3ff`: calls And/ClearMask/Divide/Xor helpers (probably another fold step). Not analysed.

## Test scaffolding (not in repo; was in the session scratchpad)
`ec_check.py` (GF + doubling vs sect233r1), `finalize_check.py` (FinishInverse contract), `mul_check.py` (literal FieldMultiply). Offer: add under `tools/` if wanted.

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
| 13a7662c | `ECC_CombineTransformA` | `ECC_PointDouble` |
| 13a76470 | `ECC_CombineAndProcess` | `ECC_PointAdd` |
| 13a76724 | `ECC_AdjustAndProcess` | `ECC_PointSubtract` (param c -> out) |
| 13a768b8 | `FUN_13a768b8(int,int,short*)` | `ECC_PointCombTable16(CurvePoint*,CurvePoint* table16,ECDHParameters*)` (first named `ECC_PointMultiplesTable16` with a wrong "k*P" comment, corrected; comment later updated to "confirmed") |
| 13a7ad3c | `ECC_RightShiftUInt233(UInt233* words)` | `ECC_RotateRightUInt233(UInt233* words)` (it is a rotate, see map above) |
| 13a7ad3c | `ECC_RotateRightUInt233(UInt233* words)` | `ECC_HalveUInt233(UInt233* words)` (2026-10-23: named after its mathematical action, k / 2) |

Also: buffer locals in `ECC_FieldFinishInverse` and `ECC_FieldMultiply` set to `unsigned int[15]`; locals renamed
(`value,m,folded,mShl74,mShl233,mask` / `reduced,m,folded,mShl74,mShl233,product,productLE` / `b_u,v,c`);
decompiler/plate comments replaced on all functions above (old ones were wrong, e.g. "salt/ctx mixing").
Not changed: `ECC_EcScalarMultiplyPointInPlace` (name/comment), `ECDHParameters.Scalar` field name, `CurvePoint`/`UInt233` definitions.

## Ghidra changes made (2026-10-21) — before -> after
| Addr | Before | After |
|---|---|---|
| 13a71fa8 | `ECC_UnpackDualPoint` (briefly `ECC_PublicKeyToPoint`)`(void* compressedPoint,int pointSize,CurvePoint* out64)` | `void ECC_LoadUInt233Pair(byte* src,int srcSize,UInt233* out)`; locals -> `first`, `second` |
| 13a72004 | `FUN_13a72004(int,int,int)` | `void ECC_StoreUInt233Pair(UInt233* in,byte* dst,int dstSize)`; locals typed `UInt233 first`, `second` |
| 13a720d0 | `ECC_PrivateKeyToScalar` | `void ECC_LoadUInt233(byte* src,UInt233* out)` |
| 13a72060 | `ECC_EncodePoint32To30(void* out30,void* point32)` | `void ECC_StoreUInt233(byte* dst,UInt233* value)` |
Comments on both list the callers (Store's comment first said "points only", corrected: 13a753fc stores ECDSA r ‖ s). Both still carry the tool-generated "Setting prototype:" header.
Side effect: `FUN_13a75428`'s locals were re-inferred as `uint auStack_160[16]` (Q) and `uint auStack_120[65]` (sig + ECDHParameters merged).
Not yet retyped: Q `CurvePoint` @sp+0x0c, sig `UInt233[2]` @sp+0x4c, params `ECDHParameters` @sp+0x8c.

## Ghidra MCP quirks
- `set_function_prototype` does **not** rename; use `rename_function`. It writes a tool-generated "Setting prototype:" header comment (goes stale after renames).
- Prototype strings can't resolve `uint`; use `unsigned int`.
- Many `set_function_prototype` calls issued in parallel can be silently lost (2 of 16 were) — re-read signatures to verify.
- `create_struct` with `size` + `members` stacks (60 + 60 = 120 bytes); `clear_struct` then `add_struct_members` gave the right size.
- Pointer-to-array params work as `unsigned int[15] *` (not `uint[15] *`, `uint` does not resolve); decompiles as `uint (*p)[15]`, like `UInt233 *`.
- Locals typed explicitly by earlier calls are not re-inferred when callee prototypes change; retype them with `set_local_variable_type` (`unsigned int[15]`).
- With the callee prototypes fixed, the mask stores (`mask[0xe] = 0xffffffff`) reappear in `ECC_FieldFinishInverse`.
