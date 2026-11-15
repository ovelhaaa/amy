# SMK-P4 0.15 SMK25 controls

The Legacy S3 profile checks the SMK25 device identity and USB cable zero.
Pad B4 (MIDI note 47, channel 10) emits one Toggle per press. Repeated
Note On while held and the matching release do not submit another Toggle.
A disconnect invalidates the queued gesture and clears the producer pad
state on device identification. The UI shows the owner's applied value.

CC114 (Play) on channel 1 arms ARP on a rising edge when enabled. CC115
(Stop) on channel 1 immediately advances the USB generation on its rising
edge. This cancels both generated and direct USB groups. CC117 (Rec)
remains observable and reserved. CC21–36, other pad shortcuts, MIDI Learn,
Pickup and long-press Save As use their existing paths.
