# Fase 2.5 â€” consolidaÃ§Ã£o temporal

Data: 2026-11-01. Auditoria iniciada em `9dcc4a8a07b99cc1071ed3cf9012a86c5a5f3336`, Ã¡rvore limpa. Comparado a `3f4054ed1fd55517cbd58c56592b786d6915e674`, somente `library.properties` difere. Branch `hardening/smk-s3-realtime`.

## Auditoria e classificaÃ§Ã£o

| Classe | EvidÃªncia | DecisÃ£o |
| --- | --- | --- |
| Defeito comprovado | Teste de 500.000 publicaÃ§Ãµes concorrentes falhou associando timestamp de outra publicaÃ§Ã£o ao tick. | Snapshot protegido por seÃ§Ã£o crÃ­tica curta no ESP; mutex no host, sem seqlock informal. |
| Risco de concorrÃªncia | Callback verificava running/source antes de Stop/reset e publicava depois; contador era reiniciado fora da mesma exclusÃ£o. | TransiÃ§Ãµes e callback compartilham exclusÃ£o temporal; APIs de timer serializadas entre tarefas, nunca chamadas sob a seÃ§Ã£o crÃ­tica. |
| Defeito comprovado por inspeÃ§Ã£o | Callback atrasada avanÃ§ava apenas um tick; timestamp novo escondia o atraso musical. | Projeta deadlines vencidos em O(1), publica somente o Ãºltimo com timestamp lÃ³gico. |
| LimitaÃ§Ã£o arquitetural | Controle executa SPIFFS/NVS/UI; console tambÃ©m acessa objetos de controle. | Isolar operaÃ§Ãµes comprovadas com ownership explÃ­cito; medir em placa. |
| AusÃªncia de mediÃ§Ã£o | Nenhuma captura MIDI/analÃ³gica ou duraÃ§Ã£o fÃ­sica de Flash nesta sessÃ£o. | MANUAL, sem inferir latÃªncia fÃ­sica por contadores internos. |
| Comportamento correto preservado | EventBus reserva releases e usa geraÃ§Ã£o/Panic; AMY owner Ãºnico, PCM geracional, notas repetidas por referÃªncias. | Manter regressÃµes existentes. |
| Conflito com premissa do pedido/relatÃ³rio anterior | `src/api.c:amy_sysclock64/amy_add_event` usa **milissegundos**, nÃ£o amostras; `src/amy.c:amy_execute_deltas` avalia antes do bloco. | Corrigir documentaÃ§Ã£o; nÃ£o declarar precisÃ£o por amostra nem mudar core AMY. |

## Entrega A â€” clock/mailbox

Todos os campos do mailbox, inclusive posiÃ§Ã£o consumida/reset, sÃ£o protegidos juntos. ClockManager usa uma seÃ§Ã£o crÃ­tica temporal para contador, deadlines, callback e transiÃ§Ãµes; getters pÃºblicos continuam atÃ´micos. API de controle usa mutex recursivo entre tarefas para serializar start/stop/reconfiguraÃ§Ã£o; callback nunca adquire esse mutex. Ordem fixa: API â†’ temporal â†’ mailbox. Nenhum log/API de timer dentro da seÃ§Ã£o crÃ­tica da callback. Reset invalida a publicaÃ§Ã£o anterior sob a mesma exclusÃ£o; nÃ£o hÃ¡ escritor antigo que sobreviva ao reset. Source swap mantÃ©m posiÃ§Ã£o musical, descarta publicaÃ§Ã£o pendente e reinicia fase/estimativa de perÃ­odo. Start reinicia posiÃ§Ã£o; Stop remove pendÃªncia. Callback residual antes do primeiro deadline novo Ã© ignorada. BPM mantÃ©m posiÃ§Ã£o e inicia novo perÃ­odo a partir da mudanÃ§a. Swing permanece nos geradores.

ValidaÃ§Ã£o e demais entregas serÃ£o registradas abaixo.

## Entrega B — política musical

Note On físico sem deadline é aplicado na primeira janela disponível (não há conhecimento do gate futuro). Geradores aceitam atraso menor que meia pulsação PPQN; a razão é configurável em `MusicalTiming`. O mínimo gate é uma pulsação. Onsets vencidos não são recuperados. Arp mantém a posição da lista quando descarta onset; sequenciador avança posições e chains por aritmética limitada, sem executar os passos perdidos. Ratchets de um onset descartado ficam desarmados. Gates vencidos liberam notas antes de novos triggers, com comparação wrap-safe.

Eventos gerados guardam origem lógica e deadline de gate até o owner. Caso expirem na FIFO/owner, Panic invalida a geração inteira e solicita limpeza dos geradores: sem ownership por origem, descartar somente o On e posteriormente executar seu Off poderia liberar uma referência de outra origem. Essa recuperação conservadora pode silenciar notas válidas sob sobrecarga e é intencional. Stop/All Notes Off agora são barreiras de geração e cancelam cena pendente; Panic também para clock e limpa a cena. Cena pendente em tick atrasado aguarda o próximo beat válido; não é recuperada em rajada.

`SynthEvent` passa de 16 para 24 bytes, ainda limitado e sem heap no roteamento; FIFO padrão cresce 2 KiB, além do alinhamento dos metadados de Entry. Não houve alteração de formato persistente. Nova espera do controle é 1 ms; a referência temporal continua sendo esp_timer/deadlines, não a contagem de delays da tarefa. Custo/prioridades reais precisam de bancada.

PASS: `./tests/run_smk_safety.ps1` (inclui testes de clock real com tempo simulado e geradores em 30/60/120/180/300 BPM, swing 50/67/75, coalescência, expiração e Panic). PASS: `./tests/run_smk_synth_expansion.ps1` após mudança de posições perdidas. Regressão AMY owner inclui expiração dentro da fila antes de executar On.
