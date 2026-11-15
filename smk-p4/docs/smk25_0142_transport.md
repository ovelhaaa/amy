# Transporte SMK25 no SMK-P4 0.14.2

| Mensagem USB MIDI, canal 1 | Ação |
|---|---|
| CC114 | Play reservado |
| CC115 | Stop aciona Panic somente da origem USB |
| CC117 | Rec reservado |

Stop atua na transição de zero para valor positivo. Valor positivo repetido conserva o estado pressionado e zero rearma. O produtor USB invalida a geração musical da origem USB, e o owner cancela suas notas e sustain. O cancelamento independe da capacidade disponível na fila de interface e conserva as vozes de toque. Os três CCs aparecem na tela MIDI.

A qualificação física deve confirmar que Stop envia zero antes de uma nova pressão e que o owner mantém as vozes de toque durante o cancelamento de uma nota USB sustentada.
