# SMK-P4 0.15 Harmony integration

The USB MIDI queue delivers original key events. With ARP enabled, the
engine captures their untransformed identities and expands the selected
octaves. A scheduled root uses the same Harmony transform as a direct
USB event. Scale, chord and voicing changes affect each new admission.
The pitch list and physical slots of an admitted group stay frozen.

Harmony filters an ineligible root or produces one to four pitches. The
worker must reserve every slot across both the Touch and USB sources before
sending any AMY Note On. At most one generated group plays at a time and the
shared physical capacity remains eight. The arpeggiator moves to the next
step when a root is filtered or its group is rejected. The preview marks
root steps and reports whether the current generated gate was admitted.
