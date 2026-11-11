# SMK-P4 0.14 — Soft Takeover

Pickup usa o valor manual confirmado. Destinos logarítmicos convertem o valor para o domínio MIDI usando os limites do registro. A UI mostra valor interno, posição física e direção no mapeamento MIDI.

| Evento | Resultado |
|---|---|
| Primeiro valor fora do alvo | Determina a direção, sem alterar o valor |
| Movimento líquido de pelo menos 3 unidades rumo à faixa de ±3 | Captura |
| Cruzamento do alvo | Captura, mantendo o valor interno no cruzamento |
| Repetição da amostra de captura | Mantém o valor interno |
| Movimento após captura | Encaminha novo valor |
| Patch, Recall, Compare/Revert, banco, binding, toque ou reconexão | Rearma pickup |

Pickup é o único modo habilitado.
