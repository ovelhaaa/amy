# User Preset format0.11

16 slots, namespace sound011, keys s00a/s00b through s15a/s15b. Each key is one
128-byte blob, explicit little endian; never memcpy a C++ preset structure.
No instrument numbers, physical oscillator addresses, pointers or ABI padding.
Float fields require IEEE754 binary32 (compile-time assertion).

|Offset|Bytes|Meaning|
|---|---:|---|
|0|4|magic0x31314553|
|4|4|format version1; unknown versions rejected|
|8|4|monotonic nonzero sequence; exhaustion rejected|
|12|4|factory ROM ID0..255|
|16|4|family0 FM /1 Juno|
|20|4|override mask,17 stable parameter IDs|
|24|25|ASCII printable name,1..24 chars, terminating NUL|
|49|3|Brightness/Attack/Release macro positions, neutral64|
|52|68|17 manual binary32 values; inactive overrides encoded zero|
|120|4|ROM compatibility0x3e76efb6 (qualified SHA256 prefix)|
|124|4|CRC32/IEEE of bytes0..123|

The fingerprint is a compatibility discriminator, not a security signature.
Current qualified ROM SHA256 is
3e76efb6519174663fb81073fa4e8332130489b991b4d3c6825535e1e8516a5e.
Future ROM changes require explicit compatibility review, not blindly accepting
same-ID factory content. Future FX require a new version/migration; no fake FX
block is stored. Firmware version changes alone do not require rewriting blobs.

Validate length, magic, version, CRC, ROM discriminator, source ID, current
SUPPORTED flag, family, masks, finite/ranged active values, name and macro ranges.
FM attack/release macros must remain neutral. Recall additionally checks the
materialized availability mask and current factory admission. A blocked factory
source cannot be enabled by a User Preset. Invalid data leaves factory service
available and the UI reports rejection/empty-invalid slot.

Save chooses an absent/invalid or older bank, leaving the newest valid record
untouched. nvs_set_blob + nvs_commit is followed by read/decode/sequence check.
Recall reads both banks and selects the valid highest sequence. CRC-corrupt,
future-format and torn copies fall back to the other valid copy. Exhausting the
sequence fails closed. Host tests simulate failed/torn writes and rebooted runtime;
this does not certify a physical power cut or ESP-IDF NVS implementation in host.


## 0.11.1 V2 integration

The V1 layout above remains a read/rollback format. New Save operations use
160-byte sparse V2 records in the separate `sound011v2` namespace. See
[Gate B storage layout, capacity and recovery](editor_0111_gate_b.md#v2-saverecall).
No automatic rewrite or erasure of V1 keys is performed.
