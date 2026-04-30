# /dev/es — ES Module Reimplementation Plan

> **IOS Reference**: IOS 58, ES binary at `0x20100000`–`0x2010b8bc`  
> **Ghidra base**: functions originally prefixed `ES_` at `0x201xxxxx` (prefix removed in reimplementation)  
> **Dependency**: /dev/fs must be fully operational before ES can be implemented  

---

## Overview

ES (E-Ticketing Service) is the security heart of IOS. It:
- Manages installed titles and tickets on NAND
- Authenticates disc and digital content via PKI certificate chains
- Controls which title is running and its access rights
- Launches titles and other IOS modules
- Exposes an ioctlv-based IPC interface (`/dev/es`) to the PPC

The module holds a pool of **3 `ESContext` slots** (each `0x1c0` bytes) and a single **`ActiveTitleContext`** for the currently-running title. All IPC requests are serialised through a single message queue + timer.

---

## Proposed File Structure

```
modules/es/source/
├── types.h                            # Step 1 – ES-specific types (TitleType, TitleID, TitleUIDEntry, ...)
├── filesystem/
│   ├── fs.c / fs.h                    # Step 1 – ES-only FS utilities (GetTitleUserId, DeleteDirectoryIfEmpty, ReadDirectoryEntries)
├── crypto/
│   ├── crypto.c / crypto.h            # Step 2 – PKI chain verify, sign, encrypt/decrypt
├── ticket/
│   ├── ticket.c / ticket.h            # Step 3 – Ticket import, views, limits
├── tmd/
│   ├── tmd.c / tmd.h                  # Step 4 – TMD storage, views, content listing
├── title/
│   ├── title.c / title.h              # Step 5 – Title enumeration and deletion
│   ├── title_import.c / title_import.h # Step 6 – Title/content install and export
├── content/
│   ├── content.c / content.h          # Step 7 – Content open/read/seek/close
├── diverify/
│   ├── diverify.c / diverify.h        # Step 8 – DI/disc verification, ImportBoot
├── launch/
│   ├── launch.c / launch.h            # Step 9 – Title and module launching
└── es.c / es.h                        # Step 10 – Core: entry, dispatch, open/close
```

The core library provides the `/dev/fs` layer shared by all modules:

| Core header/source | Contents |
|--------------------|----------|
| `<fs/types.h>` | `FileStatistics`, `SFFSStatistics`, `FileOperationsParameter`, `FileRenameParameter`, `GetAttributesParameters` |
| `<fs/errors.h>` | `FS_EINVAL`, `FS_ENOENT`, `FS_NOFILESYSTEM`, and all other FS error codes |
| `<fs/ioctls.h>` | `FSIoctlCommands` and `FSIoctlvCommands` enums |
| `<fs/fs.h>` / `core/source/fs/fs.c` | `OpenFile`, `ReadFile`, `WriteFile`, `SeekFile`, `GetFileStats`, `GetNandStatistics`, `CreateDirectory`, `CreateDirectoryRecursive`, `ReadDirectory`, `SetAttributes`, `GetAttributes`, `DeletePath`, `RenamePath`, `CreateFile`, `CreateAndWriteFile`, `SetFileVersionControl`, `ShutdownFileSystem`, `FormatFileSystem` |

The ES `filesystem/` subdirectory provides only the ES-specific higher-level utilities
that depend on ES types (`TitleUIDEntry`, `TitleType`).

---

## Step 1 — Preparation: Syscalls, Shared Types & ES FS Utilities

**Purpose**: Foundation for all subsequent steps. The kernel syscall table and core
OS-wrapper layer must be complete before any ES code can be written. `types.h` defines
all ES-specific structs and enums; `filesystem/fs.c`/`fs.h` provides the ES-only
higher-level FS utilities built on top of `<fs/fs.h>`.

### Step 1a — Missing kernel syscalls (`kernel/source/interrupt/syscall.cpp`, `kernel/source/crypto/iosc.c`, etc.)

These must land on the **main branch before any ES implementation begins**. All are
currently `SYSCALL_NULL`. The kernel implementation and the `OS*` wrapper in
`core/include/ios/syscalls.h` must both be added.

#### Non-IOSC syscalls

| Syscall # | IOS name | Kernel function to implement | `OS*` wrapper | Needed by |
|-----------|----------|------------------------------|--------------|-----------|
| 0x47 | `IOS_get_kernel_flavor` | `GetKernelFlavor(s16 *type, s16 *unk)` — copy from `IOS_WhichKernel` @ `ffff1970`: sets `type[0]=0`, `type[1]=3`, `*unk=0` | `OSGetKernelFlavor` | Steps 9/10 (ESMain, CheckLaunchSys) |
| 0x41 | `IOS_StartPPC` | `StartPPC(const char *path)` — power-on PPC, jump to content path | `OSStartPPC` | Step 9 (LaunchTitle) |
| 0x42 | `IOS_ios_boot` | `IOSBoot(const char *path)` — reload IOS from path (see `ios_boot` @ `ffff1370`) | `OSIOSBoot` | Step 9 (LaunchTitle/IOS reload) |
| 0x4D | `IOS_GetLoMemOSVersion` | `GetIosVersion(void)` — read IOS version from LoMem @ `0x80003140` (see `GetLoMemOSVersion` @ `ffff6150`) | `OSGetIosVersion` | Step 9 (CheckLaunchSys) |
| 0x54 | `SetPPCACRPerms` | `SetPPCACRPerms(u8 enable)` — set PPC ACR permission bits (see HW reg write @ `ffff` region) | `OSSetPPCACRPerms` | Step 9 (TriggerActiveTitleAccessRights) |
| 0x59 | `SetIpcAccessRights` | `SetIpcAccessRights(const u8 *rights)` — per-process IPC permission bitmask (see `SetIpcAccessRights` @ `ffff2ed4`) | `OSSetIpcAccessRights` | Step 9 (TriggerActiveTitleAccessRights) |

#### IOSC syscalls (kernel impl in `iosc.c` + syscall table entry)

| Syscall # | IOS name | Kernel function | `OS*` wrapper | Needed by |
|-----------|----------|-----------------|--------------|-----------|
| 0x5D | `IOS_IOSC_ImportSecretKey` | `IOSC_ImportSecretKey(ks, ks_verify, ks_decrypt, flags, sig, iv, key)` | `OSIOSCImportSecretKey` | Steps 3/6 |
| 0x5F | `IOS_IOSC_ImportPublicKey` | `IOSC_ImportPublicKey(data, rsa_e, keyslot)` | `OSIOSCImportPublicKey` | Step 2 |
| 0x61 | `IOS_IOSC_ComputeSharedKey` | `IOSC_ComputeSharedKey(ks_priv, ks_pub, ks_shared)` | `OSIOSCComputeSharedKey` | Steps 2/9 |
| 0x67 | `IOS_IOSC_GenerateHash` | `IOSC_GenerateHash(ctx, data, size, chain_flag, digest)` | `OSIOSCGenerateHash` | Step 2 |
| 0x6C | `IOS_IOSC_VerifyPublicKeySign` | `IOSC_VerifyPublicKeySign(data, size, keyslot, sig)` | `OSIOSCVerifyPublicKeySign` | Step 2 |
| 0x6F | `IOS_IOSC_ImportCertificate` | `IOSC_ImportCertificate(cert, ks_signer, ks_pubkey)` | `OSIOSCImportCertificate` | Step 2 |
| 0x70 | `IOS_IOSC_GetDeviceCertificate` | `IOSC_GetDeviceCertificate(ECCCert *out)` | `OSIOSCGetDeviceCertificate` | Step 2 |
| 0x71 | `IOS_IOSC_SetOwnership` | `IOSC_SetOwnership(keyslot, pid_mask)` | `OSIOSCSetOwnership` | Steps 3/6 |
| 0x72 | `IOS_IOSC_GetOwnership` | `IOSC_GetOwnership(keyslot, *ownership)` | `OSIOSCGetOwnership` | Step 2 |
| 0x74 | `IOS_IOSC_GenerateKey` | `IOSC_GenerateKey(keyslot)` | `OSIOSCGenerateKey` | Step 6 |
| 0x75 | `IOS_IOSC_GeneratePublicKeySign` | `IOSC_GeneratePublicKeySign(hash, hash_len, keyslot, sig_out)` | `OSIOSCGeneratePublicKeySign` | Step 2 |
| 0x76 | `IOS_IOSC_GenerateCertificate` | `IOSC_GenerateCertificate(keyslot, name, cert_out)` | `OSIOSCGenerateCertificate` | Step 2 |

IOS IOSC reference implementations are at `0x13a7xxxx`. The kernel `iosc.c` already
implements `IOSC_CreateObject`, `IOSC_DeleteObject`, `IOSC_SetData`, `IOSC_GetData`,
`IOSC_GetKeySize`, `IOSC_GetSignatureSize`, `IOSC_Encrypt/Decrypt(Async)`, and
`IOSC_GenerateBlockMAC(Async)`.

---

### Step 1b — ES-specific Types (`types.h`)

| Type | Description |
|------|-------------|
| `TitleType` (enum) | `SystemTitle=0x1`, `ChannelTitle=0x10000`, `GameSaveTitle=0x10001`, `SysChannelTitle=0x10002`, `WiiWareTitle=0x10004`, `DLCTitle=0x10005`, `HiddenTitle=0x10008` |
| `TitleID` (0x08 bytes) | `TitleType Type` + `u32 Id` |
| `TitleUIDEntry` (0x0c bytes) | `TitleID Title` + `u32 UserId` |
| `ESContext` (0x1c0 bytes) | Per-open-FD state: gid, uid, `TitleImportExportContext` at +0x40, `active` flag at +0x180 |
| `TitleImportExportContext` | Import/export pipeline state: tmd ptr, tmd_size, keyslot, currentContentFd, ready flag, last_ret |
| `ActiveTitleContext` | Running-title state: ticket ptr, tmd ptr, `active` flag, IPC mask |
| `Ticket` / `Ticketv0` | v0 = 0x2a4 bytes; v1 wraps v0 with extra field (ticket_size) |
| `TicketView` | 0xd8 bytes — what the PPC sees of a ticket |
| `TicketLimit` | Consumption limit entry |
| `TitleMetadata` / `TitleMetadataView` | TMD and its trimmed view for PPC queries |
| `Certificate` / `SignerCert` / `CACert` / `ECCCert` | PKI certificate chain types |
| `ContainerType` | Enum: `Ticket`, `TMD`, `DeviceCertificate` |
| `StartupDirEntry` | Array entry for `CreateStartupDirectories` (path + permissions) |

### Step 1c — ES FS Utilities (`filesystem/fs.c` / `filesystem/fs.h`)

ES-specific FS utilities that depend on ES types (`TitleUIDEntry`, `TitleType`). All
`/dev/fs` I/O goes through `<fs/fs.h>`.

| Function name | IOS Decompiled Name | Address | Core equivalent used | Notes |
|---------------|----------------------|---------|----------------------|-------|
| `GetTitleUserId` | `GetTitleUID` | `0x20109370` | `OpenFile`, `GetFileStats`, `ReadFile`, `CreateAndWriteFile` | Read/append uid in `/sys/uid.sys` |
| `DeleteDirectoryIfEmpty` | `DeleteDirIfEmpty` | `0x2010951c` | `ReadDirectory`, `DeletePath` | Walk up and remove empty parent dirs |
| `ReadDirectoryEntries` | `ReadDirQuick` | `0x201095a4` | `ReadDirectory` | Alloc + fill dir entries in one call |

### Step 1d — Core library functions used by ES

ES calls these functions from `<fs/fs.h>` directly. IOS decompiled names are shown for
cross-reference against the Ghidra listing.

| Core function | IOS ES reference address | IOS Decompiled Name |
|--------------|--------------------------|---------------------|
| `OpenFile` | `0x2010a520` | `FS_Open` |
| `ReadFile` | `0x2010a584` | `FS_Read` |
| `WriteFile` | `0x2010a590` | `IOSWrite` |
| `SeekFile` | `0x2010a578` | `IOSSeek` |
| `GetFileStats` | `0x2010a55c` | `FS_GetFileStats` |
| `ShutdownFileSystem` | `0x2010a5a6` | `FSShutdown` |
| `FormatFileSystem` | `0x20109e5c` | `FS_Format` |
| `GetNandStatistics` | `0x20109e94` | `FS_GetStats` |
| `CreateDirectory` | `0x20109ed0` | `FS_CreateDir` |
| `ReadDirectory` | `0x20109f74` | `FS_ReadDir` |
| `SetAttributes` | `0x2010a054` | `FS_SetAttr` |
| `GetAttributes` | `0x2010a10c` | `FS_GetAttr` |
| `DeletePath` | `0x2010a2c8` | `FSDelete` |
| `RenamePath` | `0x2010a344` | `FSRename` |
| `CreateFile` | `0x2010a3ec` | `FSCreateFile` |
| `SetFileVersionControl` | `0x2010a490` | `FSSetFileVerCtrl` |
| `CreateDirectoryRecursive` | `0x201091b4` | `CreateDirRecursive` — mkdir -p equivalent |
| `CreateAndWriteFile` | `0x20109280` | `CreateAndWriteFile` — create + write atomically via `/tmp` |
| `hextou32` | `0x20109148` | `hextou32` — declared in `<types.h>` |

---

## Step 2 — Crypto / PKI (`crypto/crypto.c` / `crypto/crypto.h`)

**Purpose**: All PKI certificate-chain operations, signing, signature verification,
symmetric encryption/decryption, and the runtime certificate store.

### IOS Functions in this domain

| IOS Decompiled Name | Address | Notes |
|----------------------|---------|-------|
| `CertificateSearchFunction` | `0x201068f4` | Walk a `Certificate` buffer; find cert by issuer name |
| `UpdateCertificateStore` | `0x20106a58` | Persist a verified cert to the on-NAND cert store (`/sys/cert.sys`) |
| `VerifyContainer` | `0x20106b9c` | Full PKI verification: search certs, import into IOSC, SHA-1 + RSA/ECC verify |
| `Sign` | `0x20106dd4` | **ioctlv 0x30**: ECC-sign a hash with the device key, return sig + device cert |
| `VerifySign` | `0x20106f08` | **ioctlv 0x31**: verify an ECC signature |
| `CheckKeyslotPermissions` | `0x2010651c` | Ensure the calling uid owns the requested IOSC keyslot |
| `Encrypt` | `0x20106574` | **ioctlv 0x2c**: AES-CBC encrypt a buffer using a caller-specified keyslot |
| `Decrypt` | `0x201065b0` | **ioctlv 0x2d**: AES-CBC decrypt a buffer using a caller-specified keyslot |
| `IOSCGetDeviceCertificate` | `0x2010b488` | **ioctlv 0x1e**: thin wrapper over `IOS_IOSC_GetDeviceCertificate` |
| `IOSCSetData` | `0x2010b418` | Thin wrapper over `IOS_IOSC_SetData` |
| `IOSCEncrypt` | `0x2010b828` | Thin wrapper over `IOS_IOSC_EncryptAsync` |

### Key implementation notes

- `VerifyContainer` is used by: ticket import, TMD install (`AddTitleStart`),
  and DI verification (`DiVerifyInner`). It must handle all three `ContainerType` values:
  `Ticket` (signer prefix `XS`), `TMD` (prefix `CP`), `DeviceCertificate` (prefix `MS`).
- The certificate store (`/sys/cert.sys`) is a flat array of `Certificate` entries;
  `UpdateCertificateStore` scans for an existing entry with the same name before appending.
- `Sign` calls `IOS_IOSC_GeneratePublicKeySign` with the device ECC key handle, then
  writes the device certificate (`IOSCGetDeviceCertificate`) alongside the signature.
- `CheckKeyslotPermissions` checks that `IOS_IOSC_GetOwnership(keyslot)` returns a pid
  matching `ctx->uid`; returns `ERR_KEYSLOT_ACCESS` otherwise.
- Common key IOSC handles are stored in `PTR_ES_CommonKeyHandles_20103494` (array of 2
  handles: normal + Korean).

---

## Step 3 — Ticket (`ticket/ticket.c` / `ticket/ticket.h`)

**Purpose**: All ticket lifecycle management: import from PPC, storage in NAND
(`/ticket/<hi>/<lo>.tik`), reading back, view generation, ticket-limit accounting
(time/count limits), UID assignment, v0/v1 import pipeline, deletion.

### IOS Functions in this domain

| IOS Decompiled Name | Address | Notes |
|----------------------|---------|-------|
| `ImportTicket` | `0x201020bc` | Top-level ioctlv 1 handler: depersonalise, verify, import |
| `DepersonalizeTicket` | `0x20101a18` | Zero out device-ID field for personalised tickets |
| `ImportV0Ticket` | `0x20101b60` | Store a single v0 ticket to NAND |
| `ImportV1Ticket` | `0x20101dcc` | Store a v1-format ticket |
| `ImportV0V1Ticket` | `0x201020a0` | Dispatch to v0 or v1 importer |
| `ReadTicket` | `0x20102938` | Read a ticket from NAND into buffer |
| `GetTicket` | `0x20102a60` | Alloc + read a ticket by title-id |
| `GetV0TicketViews` | `0x201075ac` | Enumerate v0 ticket views for a title |
| `GetV1TicketViews` | `0x201076f4` | Enumerate v1 ticket views |
| `GetTicketViewsInner` | `0x20107878` | Inner loop shared by both Get*TicketViews |
| `GetTicketViews` | `0x201078b8` | Combined view of v0+v1 tickets (ioctlv 0x12/0x13) |
| `GenerateTicketView` | `0x20107478` | Populate a `TicketView` from a raw `Ticketv0` |
| `GetV0TicketFromView` | `0x20107974` | Retrieve stored v0 ticket matching a view |
| `GetV1TicketFromView` | `0x20107b20` | Retrieve stored v1 ticket matching a view |
| `GetTicketFromView` | `0x20107d30` | Dispatch to v0/v1 based on view format (ioctlv 0x43/0x44) |
| `DeleteV0Ticket` | `0x20108ba8` | Delete a v0 `.tik` file |
| `DeleteV1Ticket` | `0x20108e0c` | Delete a v1 `.tik` file |
| `DeleteTicket` | `0x201090dc` | Top-level delete: dispatch + cleanup (ioctlv 0x18) |
| `TicketHasLimits` | `0x20102190` | Return true if any `TicketLimit` entry is non-zero |
| `GetTicketLimitStatus` | `0x201021b4` | Evaluate current consumption vs limit for one entry |
| `GetConsumption` | `0x2010229c` | Return limit array and remaining count (ioctlv 0x16) |
| `IsTicketLimitExpired` | `0x20102320` | Return true if *any* limit is exceeded |
| `UpdateTicketLimits` | `0x201023cc` | Persist updated limit counters back to NAND |
| `ActivateTicketLimits` | `0x20102788` | Arm the timer with the first upcoming expiry |
| `UpdatePlaytimeLimit` | `0x201028a0` | Tick-handler: decrement playtime, stop if expired |
| `SetUID` | `0x20105790` | Write the uid-map file for a title (ioctlv 0x21) |
| `GetTitleUID` | `0x20109370` | Read uid for a title from `/sys/uid.sys` |
| `HasKoreanKey` | `0x201096b8` | Check if the Korean common key slot is present (ioctlv 0x45) |

### Key implementation notes

- NAND paths: v0 ticket at `/ticket/<hi8>/<lo8>.tik`, v1 at `/ticket/<hi8>/<lo8>.tik`
  (v1 appended after v0 — check actual storage layout from `ImportV1Ticket`).
- Common-key index in the ticket header selects which IOSC common-key slot to use during
  `AddTitleStart`; slot 0 = normal, slot 1 = Korean.
- `TicketLimit` array is at a fixed offset within `Ticketv0`; the timer in `ESMain`
  fires every N ms to call `UpdatePlaytimeLimit`.
- `DepersonalizeTicket` checks `ticket->device_id != 0` and uses `IOS_IOSC_GetData`
  for the device ID to blank the field if it matches.
- `GetV0TicketFromViewInternal` (`0x20105de8`) is also used by `DIVerify` — it
  belongs here but must be visible to `diverify/`.

---

## Step 4 — TMD (`tmd/tmd.c` / `tmd/tmd.h`)

**Purpose**: Title Metadata storage, retrieval, view generation, and content-ID listing.
TMDs live at `/title/<hi>/<lo>/content/title.tmd` on NAND.

### IOS Functions in this domain

| IOS Decompiled Name | Address | Notes |
|----------------------|---------|-------|
| `GetStoredTmd` | `0x2010710c` | Open + read a full TMD from NAND (ioctlv 0x34/0x35) |
| `GenerateTMDView` | `0x201071f0` | Fill a `TitleMetadataView` from a TMD in memory (ioctlv 0x19/0x1a) |
| `GetTitleMetadataView` | `0x2010738c` | Calls `GetStoredTmd` then `GenerateTMDView` (ioctlv 0x14/0x15) |
| `GetActiveTitleTMD` | `0x2010733c` | Return the in-memory TMD of the active title (ioctlv 0x39/0x3a) |
| `GetStoredTMDContentsInner` | `0x20108030` | Iterate content records, fill optional content-ID array |
| `GetTitleContents` | `0x2010819c` | Top-level count+enumerate content IDs (ioctlv 0x10/0x11) |
| `GetStoredTMDContents` | `0x20108280` | Same but takes an in-memory TMD (ioctlv 0x32/0x33) |
| `WriteTempTmd` | `0x2010311c` | Write TMD to `/tmp/title.tmd` during install |
| `WriteTMDForNewTitle` | `0x20105aa8` | Write final TMD and optional per-title data to NAND |
| `CheckTitleVersionsAndStuff` | `0x20102c10` | Validate new TMD vs installed version, boot-index, required system version |

### Key implementation notes

- TMD path: `/title/<hi8>/<lo8>/content/title.tmd`; temp path during install: `/tmp/title.tmd`.
- `TitleMetadataView` is a subset of `TitleMetadata` without the actual content records.
- `GetStoredTMDContentsInner` accepts a `NULL` content-id array to do a count-only pass.
- `CheckTitleVersionsAndStuff` enforces that a newer TMD version cannot downgrade an
  already-installed title unless the `downgrade` flag is set in the TMD flags.
- The `ContentCount` field drives dynamic-size computations throughout:
  `tmd_size = 0x1e4 + ContentCount * 0x24`.

---

## Step 5 — Title Management (`title/title.c` / `title/title.h`)

**Purpose**: Enumerate installed titles, delete titles and their content/tickets, manage
shared content (`/shared1/*.app`), data-directory queries.

### IOS Functions in this domain

| IOS Decompiled Name | Address | Notes |
|----------------------|---------|-------|
| `GetOwnedTitles` | `0x20107d70` | List titles the caller has a valid ticket for (ioctlv 0xc/0xd) |
| `GetTitles` | `0x20107eec` | List all installed titles on NAND (ioctlv 0xe/0xf) |
| `GetSharedContents` | `0x2010836c` | List shared content hashes from `/shared1/content.map` (ioctlv 0x36/0x37) |
| `GetSharedContent` | `0x20104e04` | Lookup one shared content by SHA-1 hash |
| `CanDeleteTitle` | `0x20108548` | Refuse delete if the caller does not own the title |
| `DeleteTitleContents` | `0x2010857c` | Remove all `.app` files for a title (ioctlv 0x22) |
| `DeleteTitleContent` | `0x20108660` | Remove one content file by content-ID (ioctlv 0x3e) |
| `DeleteTitle` | `0x20108774` | Delete ticket + TMD + content dir (ioctlv 0x17) |
| `DeleteSharedContent` | `0x2010882c` | Remove a shared content and update `content.map` (ioctlv 0x38) |
| `GetDataDirectory` | `0x201090f4` | Format `/title/<hi>/<lo>/data` into caller buffer (ioctlv 0x1d) |

### Key implementation notes

- `GetTitles` enumerates `/title/` by reading each `<hi>/<lo>/content/title.tmd` that exists.
- `GetOwnedTitles` is a filtered version: only titles with a readable ticket are returned.
- Shared content map lives at `/shared1/content.map`; each entry is a 28-byte record:
  4-byte `content_id` + 20-byte SHA-1 hash + 4-byte padding.
- `DeleteTitle` is ordered: ticket(s), TMD, then the content directory.
  It must call `CanDeleteTitle` first (based on the caller's uid).
- `GetDataDirectory` does **not** create the directory; it only formats the path string.

---

## Step 6 — Title Install & Export (`title/title_import.c` / `title/title_import.h`)

**Purpose**: The complete install pipeline (AddTitle*), the export pipeline (ExportTitle*),
re-import, and all the internal helpers that keep the `TitleImportExportContext` coherent.
This is the most complex subsystem.

### IOS Functions in this domain

| IOS Decompiled Name | Address | Notes |
|----------------------|---------|-------|
| `InitTitleImportExportContext` | `0x20102b8c` | Zero-init the context, copy TMD, alloc keyslot |
| `FreeTitleImportExportContent` | `0x20102bdc` | Free allocations inside a context |
| `ResetTitleImportExportContextContentState` | `0x20102b50` | Close open content FD, free content buffer |
| `ChooseExportKeyForTitle` | `0x20102a94` | Select which IOSC key to use for export encryption |
| `AccomodateNewTitle` | `0x20102e34` | Create `/title/<hi>/<lo>/content/` dir structure |
| `FinishUpImport` | `0x20102fac` | Clean up stale import dirs before a new install |
| `VerifyFSAttr` | `0x201030b8` | Check that a NAND path has the expected permissions |
| `FUN_20104264` | `0x20104264` | Pre-install sanity check (title dir state); call by `AddTitleStart` |
| `AddTitleStart` | `0x201031ac` | **ioctlv 2**: verify TMD+cert chain, get ticket, decrypt title key, mark context ready |
| `AddContentStart` | `0x20103498` | **ioctlv 3**: open a temp content file for writing |
| `AddContentDataInner` | `0x201037f8` | Decrypt + hash one block of content data |
| `ExportContentDataInner` | `0x201036c8` | Encrypt one block during export |
| `AddContentData` | `0x2010394c` | **ioctlv 4**: receive a chunk, route to `AddContentDataInner` |
| `AddContentFinish` | `0x20103a54` | **ioctlv 5**: verify final SHA-1, rename temp to final |
| `RemoveUnusedImportContents` | `0x20103e24` | Delete any `.tmp` content files after a failed install |
| `RenameTitleImportContentDir` | `0x20103f58` | Rename `/tmp/<title>/` → `/title/<hi>/<lo>/content/` |
| `AddTitleFinish` | `0x2010402c` | **ioctlv 6**: rename dirs, install ticket, finalise |
| `AddTitleCancel` | `0x20104470` | **ioctlv 0x2f**: abort install, remove temp files |
| `FinishStaleImports` | `0x20105538` | On startup: clean any leftover `/tmp/` install dirs |
| `ReimportTitleInit` | `0x20106068` | **ioctlv 0x2b**: re-verify an already-installed title |
| `ExportTitleInit` | `0x201061ec` | **ioctlv 0x26**: start export, load + verify ticket |
| `ExportContentStart` | `0x20106370` | **ioctlv 0x27**: open a content file for reading during export |
| `ExportContentData` | `0x20106448` | **ioctlv 0x28**: read + re-encrypt a chunk for export |
| `ExportContentEnd` | `0x201064b8` | **ioctlv 0x29**: close the export content file |
| `ExportTitleDone` | `0x20106500` | **ioctlv 0x2a**: finalise export, free context |

### Key implementation notes

- The `TitleImportExportContext` is **embedded at offset +0x40** inside each `ESContext`; it
  is never heap-allocated.  
- Install flow: `AddTitleStart` → `AddContentStart` → (`AddContentData`)* → `AddContentFinish`
  → `AddTitleFinish`. Any failure at any step should call `AddTitleCancel`.
- `AddTitleStart` allocates an IOSC AES-128 keyslot (`ctx->keyslot`), imports the
  title key decrypted from the ticket using the common key.
- Content is written to `/tmp/<hi8>/<lo8>/content/<cid>.app.tmp`, then renamed on
  `AddContentFinish`.
- `AddContentDataInner` calls `IOS_IOSC_DecryptAsync` to AES-CBC-decrypt each block
  and accumulates a SHA-1 hash; final hash compared against the TMD content record.
- `ExportContentDataInner` mirrors this with `IOS_IOSC_EncryptAsync` using a per-export
  key chosen by `ChooseExportKeyForTitle`.
- `FinishStaleImports` is called once at startup from `CreateStartupDirectories`;
  it iterates `/tmp/` looking for leftover install directories.

---

## Step 7 — Content Access (`content/content.c` / `content/content.h`)

**Purpose**: Open, read, seek, and close installed content files. Also manages the
16-entry content-FD map and stream-key objects for hardware-accelerated title decryption.

### IOS Functions in this domain

| IOS Decompiled Name | Address | Notes |
|----------------------|---------|-------|
| `GetSharedContent` | `0x20104e04` | Find a shared content entry by SHA-1 (helper shared with title.c) |
| `OpenTitleContentInner` | `0x20104fb8` | Core open logic: validate permissions, open file, record in FD map |
| `OpenContent` | `0x20105140` | **ioctlv 9**: open content by index in active title (ioctlv 0x09) |
| `CurrentTitleCanAccessWhateverThing` | `0x201051c4` | Check that the requested content index is in the active title's TMD |
| `OpenTitleContent` | `0x201051f0` | **ioctlv 0x24**: open content by title-id + ticket view + content index |
| `ReadContent` | `0x20105444` | **ioctlv 0xa**: read N bytes from an opened content FD |
| `SeekContent` | `0x20105494` | **ioctlv 0x23**: seek within an opened content FD |
| `CloseContent` | `0x201054e4` | **ioctlv 0xb**: close a content FD, free map slot |
| `SetupStreamKey` | `0x201065ec` | **ioctlv 0x3c**: import a title key into an IOSC object for streaming |
| `DeleteStreamKey` | `0x201068e8` | **ioctlv 0x3d**: delete a stream-key IOSC object |

### Key implementation notes

- The content FD map (`ContentFdMap`) is an array of 16 `{fd, cid}` pairs initialised
  to `{-1, -1}` in `CreateStartupDirectories`.
- `OpenContent` uses the **active title's** TMD to resolve a content index to a file path.
- `OpenTitleContent` accepts a `TicketView` and verifies the caller can access that title.
- `SetupStreamKey` creates an IOSC AES-128 object, decrypts the title key from the
  `TicketView` using the appropriate common key, and stores the resulting handle; the
  handle index is written to the caller's output buffer.

---

## Step 8 — DI Verification (`diverify/diverify.c` / `diverify/diverify.h`)

**Purpose**: Authenticate disc content and disc-based title launches. `DIVerify` is
called by the DI (disc interface) driver to grant a disc title's access rights.
`ImportBoot` handles importing boot2/IOS from a disc image.

### IOS Functions in this domain

| IOS Decompiled Name | Address | Notes |
|----------------------|---------|-------|
| `DiVerifyInner` | `0x20105848` | Verify cert chain, ticket, TMD; populate `ActiveTitleContext` |
| `DIVerify` | `0x20105bd0` | **ioctlv 0x1c**: full DI verify + access rights + disc.sys write |
| `DiVerifyWithTicketView` | `0x20105f2c` | **ioctlv 0x3b**: variant using a pre-existing `TicketView` instead of a raw ticket |
| `GetV0TicketFromViewInternal` | `0x20105de8` | Resolve a `TicketView` → stored v0 ticket (shared with ticket/) |
| `ImportBoot` | `0x20109740` | **ioctlv 0x1f**: import boot content (boot2/IOS) from caller buffer |

### Key implementation notes

- `DIVerify` is **only callable from the DI process** (`ctx->uid == 3`), enforced in
  `HandleIoctlv`.
- After successful verification, `DIVerify` calls `TriggerActiveTitleAccessRights(false)`
  to set up IPC permissions **before** writing `disc.sys`.
- If `IsNonDiscAuthenticated == 0`, a `disc.sys` file containing the ticket view + TMD
  is written to `/sys/disc.sys` so that a future PPC-initiated `LaunchTitle` can skip
  re-verification.
- `ImportBoot` calls `DIVerify`'s inner logic then routes to `LaunchTitleInternal`;
  it requires `ctx->uid == 0` with access mode 6.
- `IOSC_CheckDiHashes` (`0x13a74abc`, in the IOSC module) is invoked from `DiVerifyInner`;
  it is a kernel-side call accessed via the syscall vector.

---

## Step 9 — Launch (`launch/launch.c` / `launch/launch.h`)

**Purpose**: Everything related to starting a title or a system image. This includes
the cold-boot path (system menu / MIOS / BC), the module-launch chain at IOS startup,
`LaunchTitle`, IOS-reload-on-version-change, and the `disc.sys` / `launch.sys`
handshake files.

### IOS Functions in this domain

| IOS Decompiled Name | Address | Notes |
|----------------------|---------|-------|
| `LaunchSystemMenu` | `0x201011c4` | Launch title-id 1-2 (System Menu) |
| `FinishBoot2Update` | `0x201011dc` | Finalise a pending boot2 update before launch |
| `LaunchBC` | `0x20101288` | Launch BC (title-id 1-100) |
| `LaunchMIOS` | `0x201012a0` | Launch MIOS (title-id 1-101) |
| `CreateStartupDirectories` | `0x201012b8` | Mkdir all required NAND paths, init content-fd map, calls `FinishStaleImports` |
| `LaunchTheRestOfModules` | `0x20101400` | Reads IOS TMD, `LaunchModule` for each non-boot shared/private content |
| `LaunchTitle` | `0x20104cf4` | High-level launch: reset ctx, `CheckLaunchSys`, set uid/gid, call `IOS_ios_boot`/`IOS_StartPPC` |
| `LaunchTitleInternal` | `0x201095f4` | Internal variant called from `ImportBoot` path |
| `CheckLaunchSys` | `0x201049c4` | Load ticket+TMD into `ActiveTitleContext`, handle `launch.sys` / IOS reload |
| `WriteLaunchAndReloadIOS` | `0x20104954` | Write `launch.sys` and trigger `IOS_ios_boot` for version-change |
| `CreateLaunchPlaceholder` | `0x20104800` | Create the 8-byte placeholder file in `/sys/` |
| `CreateLaunchFile` | `0x20104858` | Write `disc.sys` with ticket view + TMD |
| `DeleteLaunchFile` | `0x20104940` | Delete `/sys/launch.sys` or `/sys/disc.sys` |
| `GetActiveTitleBootContentPath` | `0x20104724` | Format `/title/<hi>/<lo>/content/<bootidx>.app` path |
| `GetTitleBootContent` | `0x20104674` | Get the boot content index from a stored TMD |
| `ResetActiveTitleContext` | `0x201044b0` | Zero the `ActiveTitleContext` and free owned resources |
| `LoadThingyIntoTitleContext` | `0x201044dc` | Load ticket or TMD from a NAND path into `ActiveTitleContext` |
| `TriggerActiveTitleAccessRights` | `0x20104c3c` | Apply the title's IPC mask via `SetIpcAccessRights` |
| `GetActiveTitleTMD` | `0x2010733c` | Return the active title's TMD size and/or data |

### Key implementation notes

- `CreateStartupDirectories` iterates 9 `StartupDirEntry` records
  (`PTR_ES_StartupDirEntries_201013e0`); special handling for `/meta` (set owner to
  system-menu uid) and `/wfs` (set uid/gid to `0x13`).
- `CheckLaunchSys` is the critical gatekeeper: it validates title IDs match, checks
  ticket limits, handles the `launch.sys` rendezvous for version-change boots, and finally
  populates `ctx->active = 1`.
- For IOS titles (`tid_hi == 1, tid_lo >= 2`) on a kernel flavor >= 3, the existence of
  `/sys/launch.sys` signals that this is a pending IOS reload; the file is read, deleted,
  and the TID is compared to decide between re-using it or writing a new one.
- `TriggerActiveTitleAccessRights` calls `SetIpcAccessRights` with the IPC mask from
  the TMD (`tmd->IpcMask`); the `active` argument controls whether to enable or disable.

---

## Step 10 — Core (`es.c` / `es.h`)

**Purpose**: Module entry point, IPC message loop, per-FD context pool, timer tick
for ticket-limit accounting.

### IOS Functions in this domain

| IOS Decompiled Name | Address | Notes |
|----------------------|---------|-------|
| `ESEntrypoint` | `0x20100000` | Sets thread priority, calls `ESMain` |
| `ESMain` | `0x201015e8` | Startup sequence + message loop |
| `HandleOpen` | `0x20100048` | Allocates an `ESContext` slot |
| `HandleIoctlv` | `0x201000cc` | Giant switch dispatch (cases 1–0x45) |
| `HandleClose` | `0x20101198` | Clears `ctx->active` |
| `SetProcessPriorities` | `0x20109678` | Wraps `IOS_SetThreadPriority` for a process |
| `GetProcessPriorities` | `0x20109698` | Queries thread priority |
| `IOSGetProcessID` | `0x2010b798` | Thin syscall wrapper |

### Key implementation notes

- `HandleOpen` scans a static array of 3 `ESContext` slots; returns `-6` (`ERR_MAX_OPEN`) if all busy.
- `ESMain` sets uid/gid to 0, calls `CreateStartupDirectories`, then enters the message loop.
- The timer tick advances a 64-bit playtime counter and drives `UpdatePlaytimeLimit`.
- The global `IsNonDiscAuthenticated` flag (`PTR_ES_IsNonDiscAuthenticated_20101948`) controls
  which launch path is taken after a `DIVerify`-authenticated disc boot.
- `ESContext` at +0x40 holds the embedded `TitleImportExportContext`; +0x180/0x181 are the
  `active`/`gid` fields.
- `ContentFdMap` is a 16-entry array of fd pairs (initialised to `0xFFFFFFFF`) for opened content.

```
Step 1: types.h + fs.c  (no ES deps — shared foundation)
  └── Step 2: crypto/       (needs: FS helpers, IOSC)
        └── Step 3: ticket/       (needs: crypto/, FS helpers, IOSC wrappers)
              └── Step 4: tmd/    (needs: FS helpers)
                    └── Step 5: title/           (needs: tmd/, ticket/)
                    └── Step 6: title_import/    (needs: tmd/, ticket/, crypto/)
        └── Step 7: content/      (needs: tmd/, ticket/, IOSC)
        └── Step 8: diverify/     (needs: ticket/, crypto/, launch/)
        └── Step 9: launch/       (needs: active title ctx, ticket/, tmd/, FS helpers)
              └── Step 10: es.c (core — wires everything together)
```

**Recommended implementation order**: 1 → 2 → 3 → 4 → 5 → 6 → 7 → 8 → 9 → 10

> Reason: types + FS wrappers (step 1) have no ES dependencies and are the foundation for
> everything. Crypto (step 2) unlocks all PKI-dependent domains. Ticket and TMD are
> prerequisites for all install/launch logic. Core (step 10) is last because it just
> wires up the completed pieces.

---

## IPC Command Reference Table

For quick cross-referencing during implementation of `HandleIoctlv`:

| Cmd | Symbol | File |
|-----|--------|------|
| 0x01 | `ImportTicket` | ticket/ |
| 0x02 | `AddTitleStart` | title_import/ |
| 0x03 | `AddContentStart` | title_import/ |
| 0x04 | `AddContentData` | title_import/ |
| 0x05 | `AddContentFinish` | title_import/ |
| 0x06 | `AddTitleFinish` | title_import/ |
| 0x07 | `IOS_IOSC_GetData(DEVICE_ID)` | es.c inline |
| 0x08 | `LaunchTitle` | launch/ |
| 0x09 | `OpenContent` | content/ |
| 0x0a | `ReadContent` | content/ |
| 0x0b | `CloseContent` | content/ |
| 0x0c/0x0d | `GetOwnedTitles` | title/ |
| 0x0e/0x0f | `GetTitles` | title/ |
| 0x10/0x11 | `GetTitleContents` | tmd/ |
| 0x12/0x13 | `GetTicketViews` | ticket/ |
| 0x14/0x15 | `GetTitleMetadataView` | tmd/ |
| 0x16 | `GetConsumption` | ticket/ |
| 0x17 | `DeleteTitle` | title/ |
| 0x18 | `DeleteTicket` | ticket/ |
| 0x19/0x1a | `GenerateTMDView` | tmd/ |
| 0x1b | `GenerateTicketView` | ticket/ |
| 0x1c | `DIVerify` | diverify/ |
| 0x1d | `GetDataDirectory` | title/ |
| 0x1e | `IOSCGetDeviceCertificate` | crypto/ |
| 0x1f | `ImportBoot` | diverify/ |
| 0x20 | GetCurrentTitleID (inline) | es.c |
| 0x21 | `SetUID` | ticket/ |
| 0x22 | `DeleteTitleContents` | title/ |
| 0x23 | `SeekContent` | content/ |
| 0x24 | `OpenTitleContent` | content/ |
| 0x25 | `LaunchBC` | launch/ |
| 0x26 | `ExportTitleInit` | title_import/ |
| 0x27 | `ExportContentStart` | title_import/ |
| 0x28 | `ExportContentData` | title_import/ |
| 0x29 | `ExportContentEnd` | title_import/ |
| 0x2a | `ExportTitleDone` | title_import/ |
| 0x2b | `ReimportTitleInit` | title_import/ |
| 0x2c | `Encrypt` | crypto/ |
| 0x2d | `Decrypt` | crypto/ |
| 0x2e | `IOS_IOSC_GetData(BOOT2_VERSION)` | es.c inline |
| 0x2f | `AddTitleCancel` | title_import/ |
| 0x30 | `Sign` | crypto/ |
| 0x31 | `VerifySign` | crypto/ |
| 0x32/0x33 | `GetStoredTMDContents` | tmd/ |
| 0x34/0x35 | `GetStoredTmd` | tmd/ |
| 0x36/0x37 | `GetSharedContents` | title/ |
| 0x38 | `DeleteSharedContent` | title/ |
| 0x39/0x3a | `GetActiveTitleTMD` | tmd/ |
| 0x3b | `DiVerifyWithTicketView` | diverify/ |
| 0x3c | `SetupStreamKey` | content/ |
| 0x3d | `DeleteStreamKey` | content/ |
| 0x3e | `DeleteTitleContent` | title/ |
| 0x40 | `GetV0TicketFromView` | ticket/ |
| 0x41 | `GetProcessPriorities` | es.c |
| 0x42 | `SetProcessPriorities` | es.c |
| 0x43/0x44 | `GetTicketFromView` | ticket/ |
| 0x45 | `HasKoreanKey` | ticket/ |

---

## NAND Path Reference

| Path | Purpose |
|------|---------|
| `/ticket/<hi>/<lo>.tik` | v0 ticket storage |
| `/title/<hi>/<lo>/content/title.tmd` | Installed TMD |
| `/title/<hi>/<lo>/content/<cid>.app` | Installed content |
| `/title/<hi>/<lo>/data/` | Title save data directory |
| `/tmp/<hi>/<lo>/content/<cid>.app.tmp` | In-progress install |
| `/tmp/title.tmd` | In-progress TMD |
| `/sys/cert.sys` | Certificate store |
| `/sys/uid.sys` | Title UID map |
| `/sys/launch.sys` | IOS-reload rendezvous |
| `/sys/disc.sys` | Disc-boot rendezvous |
| `/shared1/content.map` | Shared content SHA-1 map |
| `/shared1/<id>.app` | Shared content files |
| `/meta/` | System menu save data |
| `/wfs/` | Wii FS root |

---

## Notes on Unresolved `FUN_` Symbols

| Address | Likely Identity | Used By |
|---------|----------------|---------|
| `0x20104264` | `PrepareInstallDir` — creates `/tmp/<hi>/<lo>/content/` | `AddTitleStart` |
| `0x2010a208` | `FS_GetUsage` — wraps FS ioctlv for quota | `HandleIoctlv` area |

These should be decompiled from Ghidra and resolved before implementing their respective domain files.
