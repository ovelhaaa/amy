# Inventário parcial de parâmetros 0.11.1

Este inventário é parcial e não representa a auditoria integral solicitada.
Os estados abaixo referem-se ao caminho P4 real, não ao backend de demonstração.

| Parâmetro | Estado | Persistência | Evidência / limite |
|---|---|---|---|
| env.amp.decay_ms | Condicional, edição em RAM | Não; Save v1 rejeitado | AMY PCM IDs 0, 2, 3, 4, 31, 1/8 vozes |
| env.amp.sustain | Condicional, edição em RAM | Não; Save v1 rejeitado | AMY PCM IDs 0, 2, 3, 4, 31, 1/8 vozes |
| amp.vel_sens | Planejado | Não | Não há comando P4; registro corrigido |
| filter.env_amount, filter.keytrack | Condicional em RAM | N�o | PCM AMY patch 4; Gate B incompleto |
| osc.*, amp.level, amp.pan | Planejado | Não | Gate C pendente |
| fm.feedback, fm.op.attack, fm.op.decay, fm.op.vel_sens | Planejado | Não | Gate D pendente |
| LFO e matriz de modulação | Planejado | Não | Gate E pendente; CC1 existente preservado |

O inventário da 0.11 documenta os controles existentes em
`sound_editor_011_parameter_matrix.md`. Números totais de parâmetros e de
persistência v2 não foram apurados; nenhum campo novo é persistente.
