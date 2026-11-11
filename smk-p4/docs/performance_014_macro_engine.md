# SMK-P4 0.14 — motor de macros

O owner recebe as oito posições junto com a edição de síntese. A UI prepara uma imagem completa e coalesce pedidos a cada 20 ms. A confirmação exige token e geração do snapshot. O owner verifica a topologia das oito vozes antes de emitir eventos. Destinos contextuais de síntese usam a máscara de disponibilidade da voz. Um macro sem destinos disponíveis fica N/A. O seletor de algoritmo FM continua uma edição explícita.

| Macro | Neutro | Juno | DX7 | FX |
|---|---:|---|---|---|
| Character | 64 | Detune, envelope do filtro | Profundidade dos moduladores, feedback | |
| Brightness | 64 | Cutoff, ressonância | Profundidade dos moduladores | Tilt EQ no DX7 |
| Motion | 24 | | | Chorus depth, delay feedback e mix |
| Shape | 64 | Decay, sustain | Decay dos operadores 1 e 2 quando disponível | |
| Attack | 32 | Ataque e envelope do filtro | Ataque dos operadores 1 e 2 quando disponível | |
| Release | 52 | Release | | Reverb mix em ambas as famílias, reverb size no DX7 |
| Space | 36 | | | Reverb mix e size, delay mix |
| Edge | 16 | Ressonância | Feedback | Tilt EQ |

A composição soma offsets com sinal em ordem fixa sobre a mesma base e aplica clamping. Curva, polaridade, limites e profundidade são propriedades de cada rota. Os presets legados preservam seus três controles históricos como parte da base. Compare suprime as contribuições e Revert restaura as posições neutras. FX usa a configuração manual confirmada e deriva a configuração aplicada no owner. Chorus Depth produz modulação audível quando o modo de chorus está ativo.

A configuração de fábrica vive em performance_factory_routes.h. O editor de rotas pela touchscreen e rampas específicas por destino continuam pendentes. A frequência de serviço do editor limita o número de atualizações, e a qualificação física de artefatos durante movimentos rápidos permanece aberta.
