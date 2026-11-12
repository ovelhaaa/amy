# SMK-P4 0.14.1 — modelo e composição de rotas

O owner recebe edit::Request pelo mailbox SPSC, com patch, geração, token, posições e configuração completa. Valida o conjunto antes de alterar o DSP e confirma token e configuração no snapshot. Uma rejeição mantém a configuração anterior.

Há oito macros com seis slots cada. Os destinos Synth conservam IDs 0–57, com seleção restrita aos contínuos disponíveis no patch. Os seis destinos FX existentes conservam IDs, e Delay Time e três controles EQ usam os seguintes. Rotas da outra família podem permanecer na configuração e ficam inativas. Rotas ROM mantêm seu fallback em patches sem determinados componentes.

Os limites e a profundidade usam a unidade do destino. O sinal da posição em relação ao neutro passa pela curva, inversão e polaridade. O owner limita cada contribuição em relação à mesma base e soma na ordem de macro e slot. O clamp final usa o domínio canônico. Os multiplicadores legados precedem a soma Synth. FX recompõe a partir da configuração manual. A união das máscaras anterior e atual restaura destinos Synth que perderam contribuição.

Uma troca de patch começa com rotas de fábrica. Compare usa a referência ROM e suspende macros. Revert recupera a referência do preset quando há uma referência aplicável, incluindo as rotas, posições, Synth e FX. O editor envia rascunhos completos e Save/Recall usa o V4 transacional para as diferenças frente às rotas de fábrica.

A suíte Sound Editor cobre 48 rotas, destinos compartilhados, atualização nas oito vozes, neutralidade repetida, troca manual durante modulação, remoção de reverb, desativação, limites, Compare, rejeição atômica e alocação. Medições de tempo real no ESP32-P4 continuam manuais.
