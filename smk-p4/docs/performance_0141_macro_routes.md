# SMK-P4 0.14.1 — modelo e composição de rotas

O owner recebe `edit::Request` pelo mailbox SPSC. A solicitação contém patch, geração, token, posições e configuração completa de rotas. O owner valida o conjunto antes de alterar o DSP. O snapshot confirma o token e a configuração aceita. Um erro deixa a configuração anterior ativa.

Uma configuração comporta oito macros com seis slots cada. O índice da linha identifica a macro. Os destinos Synth usam os IDs 0–57 e o catálogo aceita apenas os IDs contínuos. Os seis destinos FX já existentes conservam seus IDs. Delay Time e os três controles EQ usam os IDs seguintes. A disponibilidade Synth depende da máscara do patch. Rotas para a outra família podem permanecer no mesmo conjunto, sem contribuir no patch atual. Rotas de fábrica preservam seu fallback em patches que carecem de determinados componentes.

Os limites e a profundidade usam a unidade nativa do destino. O sinal da posição em relação ao neutro passa pela curva, inversão e polaridade antes da escala pela profundidade. O owner limita cada contribuição em relação à mesma base e soma na ordem de macro e slot. O clamp final usa o domínio canônico do destino. Os multiplicadores legados precedem a soma das rotas Synth. FX recompõe a partir da configuração manual, incluindo após remoção de uma rota. A união das máscaras de edição atual e anterior garante a restauração dos destinos Synth que deixaram de receber uma contribuição.

Uma troca de patch começa no perfil de fábrica. Compare mantém a configuração editada e usa a referência ROM com macros suspensas. Revert volta ao perfil de fábrica. Uma configuração customizada só pode chegar pelo mailbox neste incremento. O backend recusa Save se o snapshot contém rotas customizadas, pois V1/V2/V3 não carregam esses dados. O editor visual e o V4 seguem pendentes.

A suíte existente `run_sound_editor.ps1` cobre 48 rotas, destinos compartilhados, atualização nas oito vozes, neutralidade repetida, troca manual durante modulação, remoção de reverb, desativação, limites, Compare, rejeição atômica e alocação. O runner já integra o CI. As medições de tempo real no ESP32-P4 permanecem manuais.
