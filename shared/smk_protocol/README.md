# SMK control link v1

Control only, no PCM. Shared portable C++17 codec, session server and
stop-and-wait client. No per-message heap allocation. Runtime I/O is in
`../smk_uart/`, owned by the S3 control task / P4 UI task, never by audio/ISR.

## Wire format

SLIP: prefix/suffix `C0`; `C0` becomes `DB DC`, `DB` becomes `DB DD`.
Empty delimiters are ignored. Invalid escape or oversized frames are dropped
through the next delimiter. A partial frame expires after100ms without bytes.

| Offset | Bytes | Meaning |
|---|---:|---|
|0|2|ASCII `SM`|
|2|1|Protocol version1|
|3|1|Type, from `Type` in smk_protocol.h|
|4|2|Command sequence, little endian|
|6|4|Nonzero P4 session, little endian|
|10|2|Payload length, little endian, maximum192|
|12|N|Explicit payload|
|12+N|2|CRC16-CCITT-FALSE, initFFFF/poly1021, over header+payload, little endian|

Maximum decoded packet206 bytes, escaped wire414 bytes. The CRC check value
for `123456789` is29B1. No compiler padding, pointers, native float or C++
objects cross the wire. Exact lengths and musical limits are validated.

## Commands and responses

| Command | Payload | Response |
|---|---|---|
|HELLO|empty; sequence0|HELLO_ACK: S3 boot ID:u32, next sequence:u16, max payload:u16|
|GET_STATE|held audition note:u8, or255 when released|STATE_SNAPSHOT154 bytes|
|SET_PATCH|factory patch ID:u16,0..255|COMMAND_ACK RECEIVED|
|SET_MACRO|index:u8,0..7; value:u8,0..127|COMMAND_ACK RECEIVED, or Unsupported when no real mappings|
|SET_MASTER_VOLUME|value:u8,0..127, gain=value/127|COMMAND_ACK RECEIVED|
|PANIC|empty|COMMAND_ACK RECEIVED|
|AUDITION_NOTE_ON|note:u8,1..127; velocity:u8,1..127|COMMAND_ACK RECEIVED|
|AUDITION_NOTE_OFF|note:u8,1..127|COMMAND_ACK RECEIVED; selective Touch release|
|GET_PATCH|factory ID:u16|PATCH_STATE: ID:u16, engine patch:u16, actual name:24 bytes|

ACK payload: status:u8 (`Received=1`, `Applied=2`) and original type:u8.
**This backend only sends RECEIVED:** admission to the EventBus, not proof of
AMY materialization or audible application. `Applied` is reserved for a future
backend that can confirm it. No new materialization APIs are used.

COMMAND_ERROR payload: reason:u8, original type:u8, next expected sequence:u16.
Reasons: Version, Session, Sequence, Payload, Unsupported, Busy.
Only Backend Busy/Unsupported errors consume/cache a valid sequence. Shape,
version and sequence errors reject the request before musical dispatch.

## State snapshot

`smk_state.h` is the explicit serializer. Header data: patch ID, AMY engine ID,
actual24-byte name, master0..127, flags, BPM×100, sample rate and block frames.
Counters: render maximum, render deadline misses, PCM starvations, EventBus
overflows, engine command drops, free internal RAM/PSRAM, note references and
MIDI events. Eight macros carry actual8-byte names,0..127 values and supported
flags. Strings are bounded and locally terminated on decode.

Flags: bit0 USB connected; bit1 recent rendered frames with no audio-task
failure; bit7 **control/requested state**, which is always set by this S3
backend. Patch/macro metadata does not assert DSP APPLIED. Audio telemetry is
observed separately; neither it nor the USB flag proves analog sound quality.
`AUDIO_DIAGNOSTICS` is sent every2s while linked, sequence0, payload38 bytes:
sample rate:u32, block frames:u16, followed by eight u32 fields: render maximum,
deadline misses, PCM starvations, I2S write errors, I2S timeouts, free internal
RAM, free PSRAM and active note references. It never acknowledges an in-flight
command. These write/starvation counters are not a direct DMA-underrun sensor.

## Retry, reconnect and notes

One immutable command in flight. Retry same sequence/payload every250ms, at
most3 retries. A cached response never repeats patch, note or Panic actions.
Changing payload under the same sequence, or sending out of order, is rejected.
New sessions start with HELLO; HELLO retries in an existing session preserve
musical state. Renew the session before16-bit sequence rollover. Server boot
identity and incompatible versions are exposed to the client.

GET_STATE polls approximately every200ms while idle. Reply timeout1500ms
triggers a fresh session. An audition note has a600ms lease renewed only by a
fresh GET_STATE carrying the same held note. A release hint heals a lost Note
Off; duplicates cannot renew the note lease. This MVP supports one touch note,
not multitouch. The P4 gesture model releases on lost touch and enforces a
defensive 10-second maximum hold even while heartbeats continue.

Stabilization 0.2 reserves the next free transaction for GET_STATE after200ms,
including under continuous macro/master/catalog traffic. Note On/Off and Panic
retain musical priority. An already in-flight command is immutable. If release
or Panic arrives while it awaits an ACK, the client cancels the old session and
sends the existing HELLO fence immediately; that selectively releases Touch.
An explicit global Panic remains latched for delivery after HELLO_ACK. No stale
command is replayed. P4 drops unsent transport frames before HELLO (counted as
TX drops); bytes already in the hardware FIFO precede the fence on the wire.
This resynchronizes a partially transmitted frame with HELLO's leading SLIP END.
The UI goes offline and obtains a fresh snapshot during this recovery. Re-press
a key if a gesture was interrupted by the session change.

No lease extension is made: lost/delayed communication may expire a held note,
and GET_STATE cannot resurrect it. Responses at/after the1500ms session timeout
are rejected before they can make the UI appear connected. Sustaining Touch
through unbounded delays is deliberately unsupported. Tests cover control
flooding,100ms response delay, one lost heartbeat ACK/retry, urgent release/Panic,
expired replies and saturated/partially transmitted UART queues.

Loss/reset of the P4 releases **only Touch**, preserving USB keys and physical
sustain. Both EventBus and AMY adapter have bounded selective-release latches
that bypass full FIFOs and invalidate queued pre-disconnect touch notes.
Global Panic still uses the established application path when explicitly sent.
Reconnect does not send global Panic. No continuous Flash writes are added.

## Builds and tests

Run from the repository root, using the already pinned PlatformIO toolchain:

```powershell
./tests/run_smk_link.ps1
./tests/run_smk_safety.ps1
./tests/run_smk_amy_boot.ps1
C:/.platformio/penv/Scripts/pio.exe run -d smk-s3 -e esp32-s3-devkitc-1
C:/.platformio/penv/Scripts/pio.exe run -d smk-s3 -e esp32-s3-link
C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-display
C:/.platformio/penv/Scripts/pio.exe run -d smk-p4 -e guition-link
```

The original environments keep UART disabled. The `*-link` environments use
the bench wiring identified and confirmed free by this unit's operator: P4
TX52/RX51, S3 TX4/RX5. See `smk-p4/docs/uart_wiring.md`. Generic Kconfig defaults
remain TX/RX=-1 and reject initialization without an explicit board profile.
Configure `CONFIG_SMK_LINK_UART`,
`CONFIG_SMK_LINK_TX_GPIO`, `CONFIG_SMK_LINK_RX_GPIO` only after checking both
board schematics and existing wiring. UART baud115200,8N1, no flow control,
RX ring2048 bytes,4 fixed TX slots with a release reserve,64-byte service
budget. TX uses the nonblocking hardware FIFO API; audio never waits for UART.

The S3 build references `shared/` and does not depend on the P4 project folder.
No new third-party dependency or AMY core change is introduced by the link.
Physical enumeration, electrical wiring, reconnect and audio impact require
separate bench checks; host tests do not certify them.
