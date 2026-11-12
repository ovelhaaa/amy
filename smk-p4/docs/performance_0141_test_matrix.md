# SMK-P4 0.14.1 — matriz de aceitação

| Gate | Critério | Estado do incremento |
|---|---|---|
| A | Auditoria de arquitetura | PASS |
| B | Compatibilidade visual legada | Painel e reset PASS em host. Restauração direta após reset e matriz PCM histórica pendentes |
| C | Modelo editável | PASS em host |
| D | Composição dinâmica | PASS no owner em host |
| E | Editor visual de rotas | NOT RUN |
| F | Valores Manual/Applied | NOT RUN |
| G | Persistência transacional de rotas | NOT RUN |
| H | Compare/Revert/Reset de rotas | NOT RUN |
| I | Destinos MIDI personalizáveis | NOT RUN |
| J | Composição de 48 rotas editadas | PASS no owner em host. Persistência pendente |
| K | Fixtures e PCM V1/V2/V3 | Matriz 0.14.1 NOT RUN |
| L | Interface LVGL do editor de rotas | NOT RUN. Painel legado testado separadamente pelo runner existente de LVGL |
| M | Regressão de tempo real | Ver resultados de host. Benchmark de 48 rotas em host. Medições físicas NOT RUN |
| N | Builds | PASS para os dois perfis do incremento |
| Hardware | Guition e PCM5102A | MANUAL, NOT RUN |

## Verificação futura na Guition

Após autorização explícita para o SHA-256 escolhido, carregar um preset Juno existente com Brightness legado diferente de 64. Verificar família, destino e posição no painel. Entrar em Compare e sair de Compare. Esperar que o painel indique bypass durante Compare e mostre a posição ativa ao voltar. Tocar a mesma passagem antes e depois, usando velocidade MIDI fixa. Confirmar reset com dois toques, verificar 64 nas três posições e confirmar que a alteração exige Save para persistir. Fazer Recall da versão anterior depois de descartar a edição e verificar o retorno da posição original. Repetir com Brightness DX7 e conferir o destino FM Depth. Durante o teste, registrar underruns, notas presas, deadlines e memória livre interna e PSRAM.
