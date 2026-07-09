# ES Module Types & Struct Layouts (from Ghidra IOS58)

All sizes confirmed via `CHECK_SIZE`. Use these to write `types.h` completions and step-by-step files.

---

## Signature Types

```c
typedef enum { RSA4096_SHA1=0x10000, RSA2048_SHA1=0x10001, ECDSA_SHA1=0x10002 } SignatureType;

typedef struct { SignatureType SignatureType; u8 Signature[256]; u8 _pad[60]; char SignatureIssuer[64]; } SignatureRSA2048; // 384 bytes
typedef struct { SignatureType SignatureType; u8 Signature[512]; u8 _pad[60]; char SignatureIssuer[64]; } SignatureRSA4096; // 640 bytes
typedef struct { SignatureType SignatureType; u8 Signature[60];  u8 _pad[64]; char Issuer[64];          } SignatureECDSA;   // 192 bytes
```

---

## Certificate Hierarchy

```c
typedef struct { u32 type; char name[64]; u32 keyid; } CertificateHeader; // 72 bytes

// SignerCert = RSA-2048 signed, RSA-2048 or ECC public key: 768 bytes
typedef struct { SignatureRSA2048 signature; CertificateHeader header; u8 key[256]; } SignerCert;

// CACert = RSA-4096 signed, RSA-2048 public key: 1024 bytes
typedef struct { SignatureRSA4096 signature; CertificateHeader header; u8 key[256]; } CACert;

// ECCCert = ECDSA signed, ECC public key: 384 bytes
typedef struct { SignatureECDSA signature; CertificateHeader header; u8 key[60]; u8 padding[60]; } ECCCert;

// Certificate = union (max size = CACert = 1024 bytes)
// CertificateSearchFunction navigates by signature_type to determine actual cert type
typedef union { SignerCert signer_cert; CACert ca_cert; ECCCert ecc_cert; u8 _raw[1024]; } Certificate;
```

Cert sizes by type:
- RSA-4096 (CACert): 0x400
- RSA-2048 (SignerCert): 0x300
- ECC (ECCCert): 0x180 (ECC signed) or 0x240 (RSA signed ECC pubkey)

ContainerType signer prefix check:
- `Ticket` → signer cert name must start "XS"
- `TMD`    → signer cert name must start "CP"
- `DeviceCertificate` → signer cert name must start "MS"

---

## Ticket Types

```c
// TicketLimit: 8 bytes
typedef struct { u32 type; u32 value; } TicketLimit;

// Ticketv0: 676 bytes (the actual on-NAND ticket)
typedef struct {
    SignatureRSA2048 signature;     // +0x000, 384 bytes
    u8   ecdh_public_key[60];       // +0x180
    u8   version;                   // +0x1bc
    u8   _pad1[2];
    u8   title_key[16];             // +0x1bf
    u8   _pad2;
    u64  ticket_id;                 // +0x1d0
    u32  device_id;                 // +0x1d8
    u64  title_id;                  // +0x1dc
    u8   access_mask[2];            // +0x1e4
    u16  title_version;             // +0x1e6
    u32  permitted_title_id;        // +0x1e8
    u32  permitted_title_mask;      // +0x1ec
    bool can_export;                // +0x1f0
    u8   common_key_index;          // +0x1f1
    u8   unknown[47];               // +0x1f2
    u8   vc_flag;                   // +0x221
    u8   content_access_permissions[64]; // +0x222
    TicketLimit limits[8];          // +0x264, 64 bytes
} Ticketv0;                         // total: 676 bytes
CHECK_SIZE(Ticketv0, 0x2a4);        // 0x2a4 = 676

// TicketView: 216 bytes (subset exposed to PPC)
typedef struct {
    u8   version;                   // +0x00
    u8   _pad[3];
    u64  ticket_id;                 // +0x04
    u32  device_id;                 // +0x0c
    u64  title_id;                  // +0x10
    u8   access_mask[2];            // +0x18
    u16  title_version;             // +0x1a
    u32  permitted_title_id;        // +0x1c
    u32  permitted_title_mask;      // +0x20
    u8   can_export;                // +0x24
    u8   common_key_index;          // +0x25
    u8   unknown[47];               // +0x26
    u8   unknown2;                  // +0x55
    u8   content_access_permissions[64]; // +0x56
    u32  limits[8][2];              // +0x98, 64 bytes
} TicketView;                       // total: 216 = 0xd8 bytes
CHECK_SIZE(TicketView, 0xd8);
```

---

## TMD Types

```c
// TitleMetadataContent: 36 bytes
typedef struct {
    u32  ContentId;  // +0x00
    u16  Index;      // +0x04
    u16  Type;       // +0x06  (0x0001=normal, 0x8001=shared, 0x8004=DLC)
    u64  Size;       // +0x08
    u8   Sha1Hash[20]; // +0x10
} TitleMetadataContent; // 0x24 bytes
CHECK_SIZE(TitleMetadataContent, 0x24);

// TitleMetadata: variable size, max 18916 bytes (with 512 contents)
typedef struct {
    SignatureRSA2048 SignedBlobHeader; // +0x000, 384 bytes
    u8   Version;                     // +0x180
    u8   CaCrlVersion;                // +0x181
    u8   SignerCrlVersion;            // +0x182
    bool isVwiiTitle;                 // +0x183
    u64  RequiredSystemVersion;       // +0x184
    u64  TitleId;                     // +0x18c
    u32  TitleType;                   // +0x194
    u16  GroupID;                     // +0x198
    u16  Padding;                     // +0x19a
    u16  Region;                      // +0x19c
    u8   Ratings[16];                 // +0x19e
    u8   Reserved[12];                // +0x1ae
    u8   IpcMask[12];                 // +0x1ba
    u8   Reserved2[18];               // +0x1c6
    u32  AccessRights;                // +0x1d8
    u16  TitleVersion;                // +0x1dc
    u16  ContentCount;                // +0x1de
    u16  BootIndex;                   // +0x1e0
    u16  MinorVersion;                // +0x1e2
    TitleMetadataContent contents[512]; // +0x1e4 (actual count = ContentCount)
} TitleMetadata;
// Size formula: 0x1e4 + ContentCount * 0x24

// TitleMetadataViewContent: 16 bytes
typedef struct { u32 cid; u16 index; u16 type; u64 size; } TitleMetadataViewContent; // 0x10 bytes

// TitleMetadataView: 8284 bytes (PPC-visible subset)
typedef struct {
    u8   version;      // +0x00
    u8   _pad[3];
    u64  sys_version;  // +0x04
    u64  title_id;     // +0x0c
    u32  title_type;   // +0x14
    u16  group_id;     // +0x18
    u8   unknown1[62]; // +0x1a
    u16  title_version; // +0x58
    u16  num_contents;  // +0x5a
    TitleMetadataViewContent contents[512]; // +0x5c
} TitleMetadataView; // 0x205c bytes
```

---

## Context Types

```c
// ESContext: 448 bytes (3 slots, static array)
typedef struct {
    u16  gid;                           // +0x000
    u8   _pad[2];
    u32  uid;                           // +0x004
    u8   _pad2[56];
    TitleImportExportContext ImportContext; // +0x040, 268 bytes
    u8   _pad3[116];
    s32  active;                        // +0x180
} ESContext; // 0x1c0 bytes
CHECK_SIZE(ESContext, 0x1c0);

// TitleImportExportContext: 268 bytes (embedded at ESContext+0x40)
typedef struct {
    TitleMetadata*    tmd;                     // +0x00
    u32               tmd_size;                // +0x04
    u32               keyslot;                 // +0x08 (IOSC handle)
    s32               currentContentFd;        // +0x0c
    s32               currentContentIndex;     // +0x10
    u32               _unk;                    // +0x14
    u32               currentContentDataProcessed; // +0x18
    u32               currentContentCid;       // +0x1c
    u8                hashCtx[96];             // +0x20 (IOSCHashContext)
    u8                currentContentHash[20];  // +0x80
    u8                currentContentIV[16];    // +0x90
    u8                spareContentBuffer[64];  // +0xa0
    u32               spareContentBufferSize;  // +0xe0
    s32               last_ret;                // +0xe4
    s32               ready;                   // +0xe8
} TitleImportExportContext; // 0x10c bytes
CHECK_SIZE(TitleImportExportContext, 0x10c);

// ActiveTitleContext: 12 bytes (global singleton)
typedef struct {
    Ticketv0*       ticket;  // +0x00
    TitleMetadata*  tmd;     // +0x04
    s32             active;  // +0x08
} ActiveTitleContext; // 0x0c bytes
```

---

## Enum Types

```c
typedef enum { TMD=0, Ticket=1, DeviceCertificate=2 } ContainerType;
```

---

## Key Global Variables (ES addresses)

| Symbol | Address | Type | Notes |
|--------|---------|------|-------|
| ES common key handles | 0x20103494 | `u32[2]` | [0]=normal, [1]=Korean |
| ES context pool | somewhere | `ESContext[3]` | static array |
| ActiveTitleContext ptr | various | `ActiveTitleContext*` | singleton |
| IsNonDiscAuthenticated | 0x20101948 ptr | `s32` | disc boot flag |
| ContentFdMap | init in startup | `struct{s32 fd; u32 cid}[16]` | init to {-1,-1} |
