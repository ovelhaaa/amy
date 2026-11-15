# SMK-P4 0.15 note ownership

The AMY worker owns Touch, UsbDirect and Arpeggiator groups. Direct USB
input Off looks up UsbDirect only. Generated groups share USB channel
tuning and metadata and release by their monotonic group identity. The
owner records the admitting block and gate length for the sole generated
group. It releases a due gate before generating a new group. A rejected
admission leaves no pending release. AMY envelope tails may finish after
a group leaves the ledger.

Physical Stop advances the USB generation independently of queue capacity.
The audio owner discards old traffic and clears USB gates, sustain and the
pattern while preserving Touch. Touchscreen Stop uses the owner command and
applies the same USB barrier. The screen reports a rejected request if the
command queue is full. It can be retried. New valid keys after Stop start a
fresh pattern. Play arms an empty pattern if ARP is enabled. Disconnect,
USB reset and USB queue release overflow invalidate USB ownership. CC120
and CC123 clear the addressed USB channel. Global Panic, patch loading and
the storage quiet barrier clear all gates and captured patterns.

The two existing producer-specific SPSC queues carry versioned ARP commands.
A command carries an action, value, token, timestamp and producer generation.
The snapshot carries a revision and a separate latest confirmation for each
producer. The UI compares its confirmation token and generation before
reporting success. The USB control monitor remains independent of musical
commands. Every owner drain is bounded by the existing per-block budgets.
