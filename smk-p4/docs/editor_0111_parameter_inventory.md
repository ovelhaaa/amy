# Inventário parcial de parâmetros 0.11.1

Este inventário é parcial e não representa a auditoria integral solicitada.
Os estados abaixo referem-se ao caminho P4 real, não ao backend de demonstração.

| Parâmetro | Estado | Persistência | Evidência / limite |
|---|---|---|---|
| env.amp.decay_ms | Conditional Juno | V2 | PCM tests from Gate A increment; Gate A qualification still separate |
| env.amp.sustain | Conditional Juno | V2 | PCM tests from Gate A increment; Gate A qualification still separate |
| amp.vel_sens | Planejado | Não | Não há comando P4; registro corrigido |
| filter.env_amount, filter.keytrack | Native EG0/EG1 and NOTE, Juno only | V2 | Gate B software suite: 4 patches, C2/C4/C6, 1/8 voices, Harmony, PCM restoration and Recall |
| osc.*, amp.level, amp.pan | Planejado | Não | Gate C pendente |
| fm.feedback, fm.op.attack, fm.op.decay, fm.op.vel_sens | Planejado | Não | Gate D pendente |
| LFO e matriz de modulação | Planejado | Não | Gate E pendente; CC1 existente preservado |

O inventário da 0.11 documenta os controles existentes em
`sound_editor_011_parameter_matrix.md`. Números totais de parâmetros e de
V2 now admits 21 engine IDs: 17 baseline and 4 new fields; physical persistence is MANUAL.
