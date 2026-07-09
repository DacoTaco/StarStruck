# ES Module State

## Files in modules/es/source/

| File | Status | Contents |
|------|--------|---------|
| `types.h` | ✅ Done | `TitleType` enum, `TitleID` struct, `TitleUIDEntry` struct |
| `es.h` | 🔶 Stub | Only `#define ES_DEVICE_NAME "/dev/es"` |
| `es.c` | 🔶 Stub | `main()` with empty IPC receive loop, sets priority to 0x79, registers nothing |
| `filesystem/fs.h` | ✅ Done | `GetTitleUserId`, `DeleteDirectoryIfEmpty`, `ReadDirectoryEntries` declarations |
| `filesystem/fs.c` | ✅ Done | `GetTitleUserId` (uid.sys r/w), `DeleteDirectoryIfEmpty` (walk+rmdir), `ReadDirectoryEntries` |
| `crypto/` | ❌ Missing | PKI chain verification, Sign, Decrypt, cert store |
| `ticket/` | ❌ Missing | Ticket import/view/limits |
| `tmd/` | ❌ Missing | TMD storage, views, content listing |
| `title/` | ❌ Missing | Title enumeration, deletion |
| `title_import.c/.h` | ❌ Missing | Title/content install + export pipeline |
| `content/` | ❌ Missing | Content open/read/seek/close |
| `diverify/` | ❌ Missing | DI/disc verification, ImportBoot |
| `launch/` | ❌ Missing | Title and module launching |

## Current Step
**Step 1 is complete.** Step 2 (crypto/) is next.

## Implementation Order (from ES_REIMPLEMENTATION_PLAN.md)
1. ✅ types.h + filesystem/ (Step 1b/1c)
2. ❌ crypto/crypto.c + crypto.h (Step 2) ← **NEXT**
3. ❌ ticket/ticket.c + ticket.h (Step 3)
4. ❌ tmd/tmd.c + tmd.h (Step 4)
5. ❌ title/title.c + title.h (Step 5)
6. ❌ title/title_import.c + title_import.h (Step 6)
7. ❌ content/content.c + content.h (Step 7)
8. ❌ diverify/diverify.c + diverify.h (Step 8)
9. ❌ launch/launch.c + launch.h (Step 9)
10. ❌ es.c (full IPC dispatch) + es.h (Step 10)

Also required before ES works:
- Step 1a: Missing kernel syscalls (see syscalls-state.md) — need to land on main first

## types.h — What's Defined
```c
typedef enum { SystemTitle=0x1, ChannelTitle=0x10000, GameSaveTitle=0x10001,
  SysChannelTitle=0x10002, WiiWareTitle=0x10004, DLCTitle=0x10005, HiddenTitle=0x10008 } TitleType;
typedef struct { TitleType Type; u32 Id; } TitleID;           // 0x08 bytes
typedef struct { TitleID Title; u32 UserId; } TitleUIDEntry;  // 0x0C bytes
```
Missing: `ESContext`, `TitleImportExportContext`, `ActiveTitleContext`, `Ticket`, `TicketView`,
`TicketLimit`, `TitleMetadata`, `TitleMetadataView`, `Certificate` family, `ContainerType`,
`StartupDirEntry` — all needed for later steps.

## es.c Current main() Pattern
```c
int main(void)
{
    u32 messageQueueMessages[8] ALIGNED(0x20) = { 0 };
    OSSetThreadPriority(0, 0x50);
    OSSetThreadPriority(0, 0x79);
    printk("$IOSVersion:  ES: %s %s 64M $", __DATE__, __TIME__);
    s32 ret = OSCreateMessageQueue((void**)&messageQueueMessages, 1);
    while (1) { OSReceiveMessage(EsMessageQueueId, &esDummyData, 0); }
}
```
Does NOT call `OSRegisterResourceManager("/dev/es", ...)` yet — that's Step 10.

## Step 2 (Crypto) Key Functions to Implement
| Function | IOS addr | Purpose |
|----------|---------|---------|
| `CertificateSearchFunction` | 0x201068f4 | Find cert by issuer in buffer |
| `UpdateCertificateStore` | 0x20106a58 | Persist cert to /sys/cert.sys |
| `VerifyContainer` | 0x20106b9c | PKI verify: cert chain + SHA-1 + RSA/ECC |
| `Sign` | 0x20106dd4 | ioctlv 0x30: ECC-sign with device key |
| `VerifySign` | 0x20106f08 | ioctlv 0x31: verify ECC signature |
| `CheckKeyslotPermissions` | 0x2010651c | Check uid owns IOSC keyslot |
| `Encrypt` | 0x20106574 | ioctlv 0x2c: AES-CBC encrypt |
| `Decrypt` | 0x201065b0 | ioctlv 0x2d: AES-CBC decrypt |
| `IOSCGetDeviceCertificate` | 0x2010b488 | ioctlv 0x1e: thin wrapper |

## NAND Paths Used by ES
| Path | Purpose |
|------|---------|
| `/sys/uid.sys` | Title UID map (TitleUIDEntry array) |
| `/sys/cert.sys` | Flat Certificate array (persistent cert store) |
| `/title/<type>/<id>/content/` | Title content files |
| `/title/<type>/<id>/data/` | Title save data |
| `/ticket/<type>/<id>/` | Ticket storage |
| `/tmp/` | Temporary staging area |

## IOS Reference Addresses
- ES binary: `0x20100000`–`0x2010b8bc` (IOS 58)
- All ES function addresses start `0x2010xxxx`
- Ghidra: functions originally prefixed `ES_`, prefix removed in reimplementation

---

## ES IPC Command Table (HandleIoctlv dispatch)

All commands handled by `HandleIoctlv` in `es.c`. Input/IO counts are the ioctlv vector counts.

| Cmd | Handler Function | File | Notes |
|-----|-----------------|------|-------|
| 0x01 | `ImportTicket` | ticket/ | |
| 0x02 | `AddTitleStart` | title_import/ | |
| 0x03 | `AddContentStart` | title_import/ | |
| 0x04 | `AddContentData` | title_import/ | |
| 0x05 | `AddContentFinish` | title_import/ | |
| 0x06 | `AddTitleFinish` | title_import/ | |
| 0x07 | `IOS_IOSC_GetData(DEVICE_ID)` | es.c inline | Returns device ID |
| 0x08 | `LaunchTitle` | launch/ | |
| 0x09 | `OpenContent` | content/ | |
| 0x0a | `ReadContent` | content/ | |
| 0x0b | `CloseContent` | content/ | |
| 0x0c | `GetOwnedTitles` (count) | title/ | Returns count only |
| 0x0d | `GetOwnedTitles` (list) | title/ | Returns list |
| 0x0e | `GetTitles` (count) | title/ | Returns count only |
| 0x0f | `GetTitles` (list) | title/ | Returns list |
| 0x10 | `GetTitleContents` (count) | tmd/ | Returns count only |
| 0x11 | `GetTitleContents` (list) | tmd/ | Returns list |
| 0x12 | `GetTicketViews` (count) | ticket/ | Returns count only |
| 0x13 | `GetTicketViews` (list) | ticket/ | Returns list |
| 0x14 | `GetTitleMetadataView` (size) | tmd/ | Returns size |
| 0x15 | `GetTitleMetadataView` (data) | tmd/ | Returns data |
| 0x16 | `GetConsumption` | ticket/ | Ticket limits |
| 0x17 | `DeleteTitle` | title/ | |
| 0x18 | `DeleteTicket` | ticket/ | |
| 0x19 | `GenerateTMDView` (size) | tmd/ | |
| 0x1a | `GenerateTMDView` (data) | tmd/ | |
| 0x1b | `GenerateTicketView` | ticket/ | |
| 0x1c | `DIVerify` | diverify/ | DI process only (uid==3) |
| 0x1d | `GetDataDirectory` | title/ | Format path only, no mkdir |
| 0x1e | `IOSCGetDeviceCertificate` | crypto/ | Thin IOSC wrapper |
| 0x1f | `ImportBoot` | diverify/ | uid==0, access mode 6 |
| 0x20 | GetCurrentTitleID (inline) | es.c | Returns active title ID |
| 0x21 | `SetUID` | ticket/ | Write uid-map entry |
| 0x22 | `DeleteTitleContents` | title/ | Remove all .app files |
| 0x23 | `SeekContent` | content/ | |
| 0x24 | `OpenTitleContent` | content/ | Takes TicketView |
| 0x25 | `LaunchBC` | launch/ | Launch BC title |
| 0x26 | `ExportTitleInit` | title_import/ | Start export |
| 0x27 | `ExportContentStart` | title_import/ | |
| 0x28 | `ExportContentData` | title_import/ | |
| 0x29 | `ExportContentEnd` | title_import/ | |
| 0x2a | `ExportTitleDone` | title_import/ | |
| 0x2b | `ReimportTitleInit` | title_import/ | Re-verify installed title |
| 0x2c | `Encrypt` | crypto/ | AES-CBC with caller keyslot |
| 0x2d | `Decrypt` | crypto/ | AES-CBC with caller keyslot |
| 0x2e | `IOS_IOSC_GetData(BOOT2_VERSION)` | es.c inline | Returns boot2 version |
| 0x2f | `AddTitleCancel` | title_import/ | Abort install |
| 0x30 | `Sign` | crypto/ | ECC-sign with device key |
| 0x31 | `VerifySign` | crypto/ | Verify ECC signature |
| 0x32 | `GetStoredTMDContents` (count) | tmd/ | In-memory TMD |
| 0x33 | `GetStoredTMDContents` (list) | tmd/ | In-memory TMD |
| 0x34 | `GetStoredTmd` (size) | tmd/ | From NAND |
| 0x35 | `GetStoredTmd` (data) | tmd/ | From NAND |
| 0x36 | `GetSharedContents` (count) | title/ | /shared1/content.map |
| 0x37 | `GetSharedContents` (list) | title/ | |
| 0x38 | `DeleteSharedContent` | title/ | Updates content.map |
| 0x39 | `GetActiveTitleTMD` (size) | tmd/ | In-memory active TMD |
| 0x3a | `GetActiveTitleTMD` (data) | tmd/ | |
| 0x3b | `DiVerifyWithTicketView` | diverify/ | Uses TicketView |
| 0x3c | `SetupStreamKey` | content/ | Import title key for streaming |
| 0x3d | `DeleteStreamKey` | content/ | Free stream-key IOSC object |
| 0x3e | `DeleteTitleContent` | title/ | Remove one content by CID |
| 0x40 | `GetV0TicketFromView` | ticket/ | |
| 0x41 | `GetProcessPriorities` | es.c | |
| 0x42 | `SetProcessPriorities` | es.c | |
| 0x43 | `GetTicketFromView` (v0) | ticket/ | |
| 0x44 | `GetTicketFromView` (v1) | ticket/ | |
| 0x45 | `HasKoreanKey` | ticket/ | Check Korean common key slot |

Note: 0x3f is not listed in the plan — likely invalid/returns ES_EINVAL.
