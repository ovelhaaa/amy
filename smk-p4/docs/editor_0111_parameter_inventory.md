# Inventário parcial de parâmetros 0.11.1

Este inventário é parcial e não representa a auditoria integral solicitada.
Os estados abaixo referem-se ao caminho P4 real, não ao backend de demonstração.

| Parâmetro | Estado | Persistência | Evidência / limite |
|---|---|---|---|
| env.amp.decay_ms | Conditional Juno | V2 | PCM tests from Gate A increment; Gate A qualification still separate |
| env.amp.sustain | Conditional Juno | V2 | PCM tests from Gate A increment; Gate A qualification still separate |
| amp.vel_sens | Native Juno velocity coefficient | V2 | Gate C: low-velocity response and full-velocity invariant tested |
| filter.env_amount, filter.keytrack | Native EG0/EG1 and NOTE, Juno only | V2 | Gate B software suite: 4 patches, C2/C4/C6, 1/8 voices, Harmony, PCM restoration and Recall |
| osc.1.level | Identified main-group relative gain | V2 | Gate C five Juno patches, 1/8 voices |
| osc.sub_level | Reachable one-octave-below pulse | V2 | Missing component is N/A |
| osc.noise_level | Reachable native noise | V2 | Enables a materialized zero-level source |
| osc.1.pulse_width | Conditional static audible main pulse | V2 | Patch 31; dynamic PWM remains N/A |
| osc.1.detune_ct | Main group native log-frequency offset | V2 | Sub and LFO retained |
| amp.level | Per-instrument gain, Juno and FM | V2 | Master independent; mute and factory reset tested |
| amp.pan | Juno output-head pan | V2 | Other native coefficients preserved |
| osc.1.unison | Planned | No | Eight-voice Harmony already fills the musical pool; no redistribution policy admitted |
| osc.1.wave, osc.1.pitch_st | Planned | No | Structural/pitch changes outside the eight Gate C controls; not qualified |
| fm.feedback, fm.op.attack, fm.op.decay, fm.op.vel_sens | Planejado | Não | Gate D pendente |
| LFO e matriz de modulação | Planejado | Não | Gate E pendente; CC1 existente preservado |

O inventário da 0.11 documenta os controles existentes em
`sound_editor_011_parameter_matrix.md` contains the baseline controls.
V2 admits 29 engine IDs: 17 baseline and 12 new fields; physical persistence is MANUAL.
