# Delay Sync

`t_ms = 60000 × numerador / (48 × BPM)`. O cálculo mantém frações até a conversão do AMY para samples inteiros em 48 kHz.

| Divisão | Numerador / 48 | 120 BPM | BPM mínimo que cabe |
|---|---:|---:|---:|
| 1/4 | 48 | 500 ms | 60 |
| 1/8 pontuada | 36 | 375 ms | 45 |
| 1/8 | 24 | 250 ms | 40 |
| 1/8 tercina | 16 | 166,67 ms | 40 |
| 1/16 pontuada | 18 | 187,5 ms | 40 |
| 1/16 | 12 | 125 ms | 40 |
| 1/16 tercina | 8 | 83,33 ms | 40 |
| 1/32 | 6 | 62,5 ms | 40 |

O limite público continua 10–1.000 ms. Se a divisão solicitada ultrapassa a faixa, o owner mantém o último tap sincronizado desse período de SYNC. Sem tap sincronizado anterior, usa o tempo manual como fallback. A divisão e o BPM solicitados permanecem configurados. A UI mostra o alvo fora da faixa junto ao tempo programado, e a sincronização retoma quando o alvo volta à faixa.

Em FREE, o tempo manual e as contribuições de macros seguem a composição 0.15. Em SYNC, o owner pausa somente as contribuições destinadas a Delay Time. As rotas, posições de macro, Mix e Feedback permanecem disponíveis. A tela FX e o Routing Editor indicam a suspensão.

O AMY troca o índice de leitura diretamente. Antes de alterar um tap com efeito ativo, o owner reduz o wet em degraus por bloco, troca a posição com a saída wet já em zero e retorna ao nível solicitado. O eco para de avançar no bloco com wet zero. A mudança preserva a linha existente e pode encurtar ou interromper a cauda de forma audível. Em bypass, a UI indica o bloqueio de FX. O DSP continua com os mesmos buffers em PSRAM, sem crossfade de taps ou alteração upstream.
